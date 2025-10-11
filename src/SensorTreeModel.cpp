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
   Cleared();
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

   Cleared();
}

Node *SensorTreeModel::AddRootNode(std::unique_ptr<Node> node)
{
   if (!node)
      return nullptr;

   Node *rawNode = node.get();
   // Insert in order received
   m_rootNodes.push_back(std::move(node));
   ItemAdded(wxDataViewItem(nullptr), CreateItemFromNode(rawNode));
   return rawNode;
}

void SensorTreeModel::ClearAll()
{
   m_rootNodes.clear();
   Cleared();
}

void SensorTreeModel::AddDataSample(const std::vector<std::string> &path, const DataValue &value,
    std::optional<DataValue> lowerThreshold,
    std::optional<DataValue> upperThreshold,
    bool failed)
{
   bool structureChanged = false;
   Node *node            = FindOrCreatePath(path, structureChanged);
   bool wasFailed        = false;
   if (node) {
      wasFailed = node->HasValue() && node->IsFailed();
      node->SetValue(value, std::move(lowerThreshold), std::move(upperThreshold), failed);
   }

   bool requireFullRefresh = false;
   if (structureChanged && !m_filterLower.IsEmpty()) {
      requireFullRefresh = true;
   }

   if (node && m_showFailuresOnly) {
      const bool isFailed = node->HasValue() && node->IsFailed();
      if (wasFailed != isFailed) {
         requireFullRefresh = true;
      }
   }

   if (requireFullRefresh) {
      Cleared();
   } else if (node) {
      wxDataViewItem item = CreateItemFromNode(node);
      ItemChanged(item);
   }
}

void SensorTreeModel::AddDataSample(const SensorData &data)
{
   AddDataSample(data.GetPath(), data.GetValue());
}

Node *SensorTreeModel::FindOrCreatePath(const std::vector<std::string> &path, bool &structureChanged)
{
   structureChanged = false;

   if (path.empty())
      return nullptr;

   // Find or create root node
   Node *current          = nullptr;
   Node *newlyCreatedRoot = nullptr;
   auto it                = std::find_if(m_rootNodes.begin(), m_rootNodes.end(),
                      [&path](const std::unique_ptr<Node> &node) {
          return node->GetName() == path[0];
       });

   if (it != m_rootNodes.end()) {
      current = it->get();
   } else {
      // Create new root node
      auto newRoot     = std::make_unique<Node>(path[0]);
      current          = newRoot.get();
      newlyCreatedRoot = current;
      structureChanged = true;
      // Insert in order received
      m_rootNodes.push_back(std::move(newRoot));
   }

   // Traverse/create the rest of the path
   struct PendingEdge
   {
      Node *parent;
      Node *child;
      bool parentWasLeaf;
   };
   std::vector<PendingEdge> pendingEdges;

   for (size_t i = 1; i < path.size(); ++i) {
      Node *child = current->FindChild(path[i]);
      if (!child) {
         const bool parentWasLeaf = current->IsLeaf();
         auto newChild            = std::make_unique<Node>(path[i]);
         child                    = current->AddChild(std::move(newChild));
         structureChanged         = true;
         pendingEdges.push_back({current, child, parentWasLeaf});
      }
      current = child;
   }

   if (newlyCreatedRoot) {
      ItemAdded(wxDataViewItem(nullptr), CreateItemFromNode(newlyCreatedRoot));
   }

   for (const auto &edge : pendingEdges) {
      wxDataViewItem parentItem = edge.parent ? CreateItemFromNode(edge.parent) : wxDataViewItem(nullptr);
      wxDataViewItem childItem  = CreateItemFromNode(edge.child);
      ItemAdded(parentItem, childItem);
      if (edge.parentWasLeaf) {
         ItemChanged(parentItem);
      }
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

      ItemChanged(CreateItemFromNode(node));
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

bool SensorTreeModel::IsNodeVisible(const Node *node) const
{
   if (!node)
      return false;

   bool childVisible = false;
   for (const auto &child : node->GetChildren()) {
      if (IsNodeVisible(child.get())) {
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