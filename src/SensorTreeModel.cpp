#include "SensorTreeModel.h"

#include <algorithm>
#include <functional>
#include <utility>

SensorTreeModel::SensorTreeModel()
{
}

SensorTreeModel::~SensorTreeModel()
{
}

void SensorTreeModel::SetShowFailuresOnly(bool showFailuresOnly)
{
   if (m_showFailuresOnly == showFailuresOnly)
      return;

   m_showFailuresOnly = showFailuresOnly;
   UpdateVisibility(nullptr, {}, true);
}

void SensorTreeModel::SetFilter(const wxString &filterText)
{
   wxString trimmed = filterText;
   trimmed.Trim(true);
   trimmed.Trim(false);

   wxString lower = trimmed.Lower();
   if (lower == m_filterLower)
      return;

   m_filter      = trimmed;
   m_filterLower = lower;

   UpdateVisibility(nullptr, {}, true);
}

void SensorTreeModel::AddDataSample(const std::vector<std::string> &path, const DataValue &value,
    std::optional<DataValue> lowerThreshold,
    std::optional<DataValue> upperThreshold,
    bool failed)
{
   std::vector<CreatedEdge> createdEdges;
   bool structureChanged = false;
   Node *node            = FindOrCreatePath(path, structureChanged, createdEdges);

   if (!node)
      return;

   node->SetValue(value, std::move(lowerThreshold), std::move(upperThreshold), failed);

   std::vector<Node *> parentsToRefresh;
   parentsToRefresh.reserve(createdEdges.size());
   for (const auto &edge : createdEdges) {
      if (edge.parent && edge.parentWasLeaf) {
         parentsToRefresh.push_back(edge.parent);
      }
   }

   UpdateVisibility(node, parentsToRefresh, false);
}

void SensorTreeModel::AddDataSample(const SensorData &data)
{
   AddDataSample(data.GetPath(), data.GetValue());
}

Node *SensorTreeModel::FindOrCreatePath(const std::vector<std::string> &path, bool &structureChanged, std::vector<CreatedEdge> &createdEdges)
{
   structureChanged = false;
   createdEdges.clear();

   if (path.empty())
      return nullptr;

   // Find or create root node
   Node *current = nullptr;
   auto it       = std::find_if(m_rootNodes.begin(), m_rootNodes.end(),
             [&path](const std::unique_ptr<Node> &node) {
          return node->GetName() == path[0];
       });

   if (it != m_rootNodes.end()) {
      current = it->get();
   } else {
      // Create new root node
      auto newRoot     = std::make_unique<Node>(path[0]);
      current          = newRoot.get();
      structureChanged = true;
      // Insert in order received
      m_rootNodes.push_back(std::move(newRoot));
      createdEdges.push_back({nullptr, current, false});
   }

   // Traverse/create the rest of the path
   for (size_t i = 1; i < path.size(); ++i) {
      Node *child = current->FindChild(path[i]);
      if (!child) {
         const bool parentWasLeaf = current->IsLeaf();
         auto newChild            = std::make_unique<Node>(path[i]);
         child                    = current->AddChild(std::move(newChild));
         structureChanged         = true;
         createdEdges.push_back({current, child, parentWasLeaf});
      }
      current = child;
   }

   return current;
}

// wxDataViewModel interface implementation
unsigned int SensorTreeModel::GetColumnCount() const
{
   return COL_COUNT;
}

wxString SensorTreeModel::GetColumnType(unsigned int col) const
{
   return "string";
}

void SensorTreeModel::GetValue(wxVariant &variant, const wxDataViewItem &item, unsigned int col) const
{
   Node *node = GetNodeFromItem(item);
   if (!node)
      return;

   switch (col) {
      case COL_NAME:
         variant = wxString(node->GetName());
         break;
      case COL_VALUE:
         if (node->HasValue()) {
            variant = wxString(node->GetValue().GetDisplayString());
         } else {
            variant = wxString("");
         }
         break;
      case COL_LOWER_THRESHOLD:
         if (node->HasValue() && node->GetLowerThreshold()) {
            variant = wxString(node->GetLowerThreshold()->GetDisplayString());
         } else {
            variant = wxString("");
         }
         break;
      case COL_UPPER_THRESHOLD:
         if (node->HasValue() && node->GetUpperThreshold()) {
            variant = wxString(node->GetUpperThreshold()->GetDisplayString());
         } else {
            variant = wxString("");
         }
         break;
      case COL_ELAPSED:
         if (node->HasValue()) {
            double seconds = node->GetSecondsSinceUpdate();
            variant        = wxString::Format("%.1f", seconds);
         } else {
            variant = wxString("");
         }
         break;
      case COL_STATUS:
         if (node->HasValue()) {
            variant = wxString(node->IsFailed() ? "Failed" : "OK");
         } else {
            variant = wxString("");
         }
         break;
      default:
         variant = wxString("");
   }
}

bool SensorTreeModel::SetValue(const wxVariant &variant, const wxDataViewItem &item, unsigned int col)
{
   // For now, make it read-only
   return false;
}

bool SensorTreeModel::GetAttr(const wxDataViewItem &item, unsigned int col, wxDataViewItemAttr &attr) const
{
   Node *node = GetNodeFromItem(item);
   if (!node)
      return false;

   if (node->HasValue() && node->IsFailed()) {
      switch (col) {
         case COL_VALUE:
         case COL_LOWER_THRESHOLD:
         case COL_UPPER_THRESHOLD:
         case COL_STATUS:
            attr.SetBold(true);
            attr.SetColour(*wxRED);
            return true;
         default:
            break;
      }
   }

   if (m_filterLower.IsEmpty())
      return false;

   if (col != COL_NAME)
      return false;

   if (!NodeMatchesHighlightFilter(node))
      return false;

   attr.SetBold(true);
   attr.SetColour(*wxBLUE);
   return true;
}

wxDataViewItem SensorTreeModel::GetParent(const wxDataViewItem &item) const
{
   Node *node = GetNodeFromItem(item);
   if (!node || !node->GetParent())
      return wxDataViewItem(nullptr);

   return CreateItemFromNode(node->GetParent());
}

bool SensorTreeModel::IsContainer(const wxDataViewItem &item) const
{
   if (!item.IsOk())
      return true;

   Node *node = GetNodeFromItem(item);
   return node && HasVisibleChildren(node);
}

bool SensorTreeModel::HasContainerColumns(const wxDataViewItem &item) const
{
   return true;
}

unsigned int SensorTreeModel::GetChildren(const wxDataViewItem &parent, wxDataViewItemArray &array) const
{
   Node *parentNode = GetNodeFromItem(parent);

   if (!parentNode) {
      // Root level - return root nodes
      unsigned int count = 0;
      for (const auto &node : m_rootNodes) {
         if (IsNodeVisible(node.get())) {
            array.Add(CreateItemFromNode(node.get()));
            ++count;
         }
      }
      return count;
   } else {
      // Return children of the specified parent
      const auto &children = parentNode->GetChildren();
      unsigned int count   = 0;
      for (const auto &child : children) {
         if (IsNodeVisible(child.get())) {
            array.Add(CreateItemFromNode(child.get()));
            ++count;
         }
      }
      return count;
   }
}

void SensorTreeModel::RefreshElapsedTimes()
{
   std::function<void(Node *)> refresh = [&](Node *node) {
      if (!node)
         return;

      if (IsNodeVisible(node)) {
         ItemChanged(CreateItemFromNode(node));
      }
      for (const auto &child : node->GetChildren()) {
         refresh(child.get());
      }
   };

   for (const auto &root : m_rootNodes) {
      refresh(root.get());
   }
}

// Helper methods
Node *SensorTreeModel::GetNodeFromItem(const wxDataViewItem &item) const
{
   return static_cast<Node *>(item.GetID());
}

wxDataViewItem SensorTreeModel::CreateItemFromNode(Node *node) const
{
   return wxDataViewItem(static_cast<void *>(node));
}

bool SensorTreeModel::ShouldNodeBeVisible(const Node *node) const
{
   if (!node)
      return false;

   bool childVisible = false;
   for (const auto &child : node->GetChildren()) {
      if (ShouldNodeBeVisible(child.get())) {
         childVisible = true;
      }
   }

   if (m_showFailuresOnly) {
      const bool nodeFailed = node->HasValue() && node->IsFailed();
      if (!nodeFailed && !childVisible)
         return false;
   }

   if (m_filterLower.IsEmpty())
      return true;

   if (NodeMatchesFilter(node))
      return true;

   return childVisible;
}

bool SensorTreeModel::NodeMatchesFilter(const Node *node) const
{
   if (!node || m_filterLower.IsEmpty())
      return true;

   wxString path = wxString::FromUTF8(node->GetFullPath("/").c_str());
   return path.Lower().Find(m_filterLower) != wxNOT_FOUND;
}

bool SensorTreeModel::NodeMatchesHighlightFilter(const Node *node) const
{
   if (!node || m_filterLower.IsEmpty())
      return false;

   wxString name = wxString::FromUTF8(node->GetName().c_str());
   return name.Lower().Find(m_filterLower) != wxNOT_FOUND;
}

bool SensorTreeModel::HasVisibleChildren(const Node *node) const
{
   if (!node)
      return false;

   for (const auto &child : node->GetChildren()) {
      if (IsNodeVisible(child.get()))
         return true;
   }
   return false;
}

bool SensorTreeModel::IsNodeVisible(const Node *node) const
{
   return node && m_visibleNodes.find(node) != m_visibleNodes.end();
}

void SensorTreeModel::CollectVisibleNodes(Node *node, std::vector<Node *> &out) const
{
   if (!node)
      return;

   if (ShouldNodeBeVisible(node)) {
      out.push_back(node);
   }

   for (const auto &child : node->GetChildren()) {
      CollectVisibleNodes(child.get(), out);
   }
}

void SensorTreeModel::UpdateVisibility(Node *valueChanged, const std::vector<Node *> &forceRefreshParents, bool refreshAllVisible)
{
   std::vector<Node *> newVisibleList;
   newVisibleList.reserve(m_visibleNodes.size() + 8);
   for (const auto &root : m_rootNodes) {
      CollectVisibleNodes(root.get(), newVisibleList);
   }

   std::unordered_set<const Node *> newVisibleSet;
   newVisibleSet.reserve(newVisibleList.size());
   for (Node *node : newVisibleList) {
      newVisibleSet.insert(node);
   }

   std::vector<Node *> removedNodes;
   removedNodes.reserve(m_visibleNodes.size());
   for (const Node *node : m_visibleNodes) {
      if (!newVisibleSet.count(node)) {
         removedNodes.push_back(const_cast<Node *>(node));
      }
   }

   std::sort(removedNodes.begin(), removedNodes.end(),
       [](Node *lhs, Node *rhs) {
          return lhs->GetDepth() > rhs->GetDepth();
       });

   for (Node *node : removedNodes) {
      m_visibleNodes.erase(node);
      wxDataViewItem parentItem = node->GetParent() ? CreateItemFromNode(node->GetParent()) : wxDataViewItem(nullptr);
      ItemDeleted(parentItem, CreateItemFromNode(node));
   }

   std::vector<Node *> addedNodes;
   addedNodes.reserve(newVisibleList.size());
   for (Node *node : newVisibleList) {
      if (!m_visibleNodes.count(node)) {
         addedNodes.push_back(node);
      }
   }

   std::sort(addedNodes.begin(), addedNodes.end(),
       [](Node *lhs, Node *rhs) {
          return lhs->GetDepth() < rhs->GetDepth();
       });

   for (Node *node : addedNodes) {
      m_visibleNodes.insert(node);
      wxDataViewItem parentItem = node->GetParent() ? CreateItemFromNode(node->GetParent()) : wxDataViewItem(nullptr);
      ItemAdded(parentItem, CreateItemFromNode(node));
   }

   std::unordered_set<const Node *> addedSet;
   addedSet.reserve(addedNodes.size());
   for (Node *node : addedNodes) {
      addedSet.insert(node);
   }

   if (valueChanged && newVisibleSet.count(valueChanged) && !addedSet.count(valueChanged)) {
      ItemChanged(CreateItemFromNode(valueChanged));
   }

   std::unordered_set<const Node *> refreshed;
   refreshed.reserve(forceRefreshParents.size());
   for (Node *parent : forceRefreshParents) {
      if (!parent)
         continue;
      if (!newVisibleSet.count(parent))
         continue;
      if (addedSet.count(parent))
         continue;
      if (refreshed.count(parent))
         continue;

      ItemChanged(CreateItemFromNode(parent));
      refreshed.insert(parent);
   }

   if (refreshAllVisible) {
      for (Node *node : newVisibleList) {
         if (!newVisibleSet.count(node))
            continue;
         if (!m_visibleNodes.count(node))
            continue;
         if (addedSet.count(node))
            continue;
         if (refreshed.count(node))
            continue;

         ItemChanged(CreateItemFromNode(node));
      }
   }

   m_visibleNodes = std::move(newVisibleSet);
}