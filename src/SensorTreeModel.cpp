#include "SensorTreeModel.h"

#include <algorithm>
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

void SensorTreeModel::SetExpansionQuery(std::function<bool(const Node *)> query)
{
   m_isNodeExpanded = std::move(query);
}

void SensorTreeModel::AddDataSample(const std::vector<std::string> &path, const DataValue &value,
    std::optional<DataValue> lowerThreshold,
    std::optional<DataValue> upperThreshold,
    bool failed)
{
   if (path.empty())
      return;

   std::vector<Node *> existingPath;
   existingPath.reserve(path.size());

   Node *current = nullptr;
   for (size_t i = 0; i < path.size(); ++i) {
      Node *next = nullptr;
      if (i == 0) {
         auto it = std::find_if(m_rootNodes.begin(), m_rootNodes.end(),
             [&path](const std::unique_ptr<Node> &root) {
                return root->GetName() == path[0];
             });
         if (it != m_rootNodes.end()) {
            next = it->get();
         }
      } else if (current) {
         next = current->FindChild(path[i]);
      }

      if (!next)
         break;

      existingPath.push_back(next);
      current = next;
   }

   std::vector<bool> beforeExisting;
   beforeExisting.reserve(existingPath.size());
   for (Node *existingNode : existingPath) {
      beforeExisting.push_back(IsNodeVisible(existingNode));
   }

   std::vector<CreatedEdge> createdEdges;
   bool structureChanged = false;
   Node *node            = FindOrCreatePath(path, structureChanged, createdEdges);

   if (!node)
      return;

   std::vector<Node *> fullPath = BuildPath(node);

   std::vector<bool> beforeStates;
   beforeStates.reserve(fullPath.size());
   for (size_t i = 0; i < fullPath.size(); ++i) {
      if (i < beforeExisting.size() && fullPath[i] == existingPath[i]) {
         beforeStates.push_back(beforeExisting[i]);
      } else {
         beforeStates.push_back(false);
      }
   }

   node->SetValue(value, std::move(lowerThreshold), std::move(upperThreshold), failed);

   std::vector<bool> afterStates;
   afterStates.reserve(fullPath.size());
   for (Node *pathNode : fullPath) {
      afterStates.push_back(IsNodeVisible(pathNode));
   }

   // Remove nodes that are no longer visible starting from the deepest node.
   for (size_t idx = fullPath.size(); idx-- > 0;) {
      if (beforeStates[idx] && !afterStates[idx]) {
         Node *currentNode         = fullPath[idx];
         wxDataViewItem parentItem = currentNode->GetParent() ? CreateItemFromNode(currentNode->GetParent()) : wxDataViewItem(nullptr);
         ItemDeleted(parentItem, CreateItemFromNode(currentNode));
      }
   }

   // Add nodes that became visible, starting from the root to ensure parents exist first.
   for (size_t idx = 0; idx < fullPath.size(); ++idx) {
      if (!beforeStates[idx] && afterStates[idx]) {
         Node *currentNode         = fullPath[idx];
         wxDataViewItem parentItem = currentNode->GetParent() ? CreateItemFromNode(currentNode->GetParent()) : wxDataViewItem(nullptr);
         ItemAdded(parentItem, CreateItemFromNode(currentNode));
      }
   }

   // Refresh the node whose data changed if it remains visible.
   if (!fullPath.empty()) {
      size_t leafIndex = fullPath.size() - 1;
      if (beforeStates[leafIndex] && afterStates[leafIndex]) {
         ItemChanged(CreateItemFromNode(fullPath[leafIndex]));
      }
   }

   if (structureChanged) {
      for (const auto &edge : createdEdges) {
         if (edge.parent && edge.parentWasLeaf && IsNodeVisible(edge.parent)) {
            ItemChanged(CreateItemFromNode(edge.parent));
         }
      }
   }
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
      case COL_VALUE: {
         if (node->HasValue()) {
            variant = wxString(node->GetValue().GetDisplayString());
            break;
         }

         // Collapsed nodes with children failures show the failed count in red
         const size_t failureCount = CountFailedDescendants(node);
         const bool isExpanded     = m_isNodeExpanded ? m_isNodeExpanded(node) : false;
         if (failureCount > 0 && !isExpanded) {
            variant = wxString::Format("%zu failed", failureCount);
         } else {
            variant = wxString("");
         }
         break;
      }
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
            attr.SetColour(*wxRED);
            return true;
         default:
            break;
      }
   }

   if (col == COL_VALUE) {
      // Collapsed nodes with children failures show the failed count in red
      const bool isExpanded     = m_isNodeExpanded ? m_isNodeExpanded(node) : false;
      const size_t failureCount = CountFailedDescendants(node);
      if (failureCount > 0 && !isExpanded && !node->HasValue()) {
         attr.SetColour(*wxRED);
         return true;
      }
   }

   if (m_filterLower.IsEmpty())
      return false;

   if (col != COL_NAME)
      return false;

   if (!NodeNameMatchesFilter(node))
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

void SensorTreeModel::Clear()
{
   m_rootNodes.clear();
   Cleared();
}

Node *SensorTreeModel::FindNodeByPath(const std::vector<std::string> &path) const
{
   if (path.empty())
      return nullptr;

   Node *current = nullptr;
   for (size_t idx = 0; idx < path.size(); ++idx) {
      const std::string &segment = path[idx];
      if (idx == 0) {
         auto it = std::find_if(m_rootNodes.begin(), m_rootNodes.end(),
             [&segment](const std::unique_ptr<Node> &root) {
                return root && root->GetName() == segment;
             });
         if (it == m_rootNodes.end())
            return nullptr;
         current = it->get();
      } else {
         if (!current)
            return nullptr;
         current = current->FindChild(segment);
         if (!current)
            return nullptr;
      }
   }

   return current;
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

std::vector<Node *> SensorTreeModel::BuildPath(Node *node) const
{
   std::vector<Node *> reversed;
   Node *current = node;
   while (current) {
      reversed.push_back(current);
      current = current->GetParent();
   }

   return std::vector<Node *>(reversed.rbegin(), reversed.rend());
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

   wxString path = wxString::FromUTF8(node->GetFullPath().c_str());
   return path.Lower().Find(m_filterLower) != wxNOT_FOUND;
}

bool SensorTreeModel::NodeNameMatchesFilter(const Node *node) const
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

size_t SensorTreeModel::CountFailedDescendants(const Node *node) const
{
   if (!node)
      return 0;

   size_t count = 0;
   for (const auto &child : node->GetChildren()) {
      const Node *childPtr = child.get();
      if (!childPtr)
         continue;

      if (childPtr->HasValue() && childPtr->IsFailed())
         ++count;

      count += CountFailedDescendants(childPtr);
   }

   return count;
}