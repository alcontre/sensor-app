#include "SensorTreeModel.h"

#include <algorithm>
#include <functional>

SensorTreeModel::SensorTreeModel()
{
}

SensorTreeModel::~SensorTreeModel()
{
}

Node *SensorTreeModel::AddRootNode(std::unique_ptr<Node> node)
{
   if (!node)
      return nullptr;

   Node *rawNode = node.get();
   // Insert in sorted order
   auto it = std::lower_bound(m_rootNodes.begin(), m_rootNodes.end(), node,
       [](const std::unique_ptr<Node> &a, const std::unique_ptr<Node> &b) {
          return a->GetName() < b->GetName();
       });
   m_rootNodes.insert(it, std::move(node));
   ItemAdded(wxDataViewItem(nullptr), CreateItemFromNode(rawNode));
   return rawNode;
}

void SensorTreeModel::ClearAll()
{
   m_rootNodes.clear();
   Cleared();
}

void SensorTreeModel::AddDataSample(const std::vector<std::string> &path, const DataValue &value)
{
   Node *node = FindOrCreatePath(path);
   if (node) {
      node->SetValue(value);
      // Notify that the item changed
      wxDataViewItem item = CreateItemFromNode(node);
      ItemChanged(item);
   }
}

void SensorTreeModel::AddDataSample(const SensorData &data)
{
   AddDataSample(data.GetPath(), data.GetValue());
}

Node *SensorTreeModel::FindOrCreatePath(const std::vector<std::string> &path)
{
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
      // Insert in sorted order
      auto insertIt = std::lower_bound(m_rootNodes.begin(), m_rootNodes.end(), newRoot,
          [](const std::unique_ptr<Node> &a, const std::unique_ptr<Node> &b) {
             return a->GetName() < b->GetName();
          });
      m_rootNodes.insert(insertIt, std::move(newRoot));
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
      case COL_ELAPSED:
         if (node->HasValue()) {
            double seconds = node->GetSecondsSinceUpdate();
            variant        = wxString::Format("%.1f", seconds);
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
   return node && !node->IsLeaf();
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
      for (const auto &node : m_rootNodes) {
         array.Add(CreateItemFromNode(node.get()));
      }
      return m_rootNodes.size();
   } else {
      // Return children of the specified parent
      const auto &children = parentNode->GetChildren();
      for (const auto &child : children) {
         array.Add(CreateItemFromNode(child.get()));
      }
      return children.size();
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