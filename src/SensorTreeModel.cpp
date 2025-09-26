#include "SensorTreeModel.h"
#include <algorithm>

SensorTreeModel::SensorTreeModel()
{
}

SensorTreeModel::~SensorTreeModel()
{
}

void SensorTreeModel::SetRootNodes(const std::vector<std::shared_ptr<Node>>& nodes)
{
    m_rootNodes = nodes;
    Cleared();
}

void SensorTreeModel::AddRootNode(std::shared_ptr<Node> node)
{
    if (node)
    {
        m_rootNodes.push_back(node);
        ItemAdded(wxDataViewItem(nullptr), CreateItemFromNode(node.get()));
    }
}

void SensorTreeModel::ClearAll()
{
    m_rootNodes.clear();
    Cleared();
}

void SensorTreeModel::AddDataSample(const std::vector<std::string>& path, const DataValue& value)
{
    auto node = FindOrCreatePath(path);
    if (node)
    {
        node->SetValue(value);
        // Notify that the item changed
        wxDataViewItem item = CreateItemFromNode(node.get());
        ItemChanged(item);
    }
}

void SensorTreeModel::AddDataSample(std::shared_ptr<SensorData> data)
{
    if (data)
    {
        AddDataSample(data->GetPath(), data->GetValue());
    }
}

std::shared_ptr<Node> SensorTreeModel::FindOrCreatePath(const std::vector<std::string>& path)
{
    if (path.empty())
        return nullptr;
    
    // Find or create root node
    std::shared_ptr<Node> current = nullptr;
    auto it = std::find_if(m_rootNodes.begin(), m_rootNodes.end(),
        [&path](const std::shared_ptr<Node>& node) {
            return node->GetName() == path[0];
        });
    
    if (it != m_rootNodes.end())
    {
        current = *it;
    }
    else
    {
        // Create new root node
        current = std::make_shared<Node>(path[0]);
        m_rootNodes.push_back(current);
        // Notify about the new root node
        ItemAdded(wxDataViewItem(nullptr), CreateItemFromNode(current.get()));
    }
    
    // Traverse/create the rest of the path
    for (size_t i = 1; i < path.size(); ++i)
    {
        auto child = current->FindChild(path[i]);
        if (!child)
        {
            child = std::make_shared<Node>(path[i]);
            current->AddChild(child);
            // Notify that a new item was added
            ItemAdded(CreateItemFromNode(current.get()), CreateItemFromNode(child.get()));
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

void SensorTreeModel::GetValue(wxVariant& variant, const wxDataViewItem& item, unsigned int col) const
{
    Node* node = GetNodeFromItem(item);
    if (!node)
        return;
    
    switch (col)
    {
        case COL_NAME:
            variant = wxString(node->GetName());
            break;
        case COL_VALUE:
            if (node->HasValue())
            {
                variant = wxString(node->GetValue().GetDisplayString());
            }
            else
            {
                variant = wxString("");
            }
            break;
        case COL_TYPE:
            if (node->HasValue())
            {
                variant = wxString(node->GetValue().IsNumeric() ? "Number" : "String");
            }
            else
            {
                variant = wxString("Container");
            }
            break;
        default:
            variant = wxString("");
    }
}

bool SensorTreeModel::SetValue(const wxVariant& variant, const wxDataViewItem& item, unsigned int col)
{
    // For now, make it read-only
    return false;
}

wxDataViewItem SensorTreeModel::GetParent(const wxDataViewItem& item) const
{
    Node* node = GetNodeFromItem(item);
    if (!node || !node->GetParent())
        return wxDataViewItem(nullptr);
    
    return CreateItemFromNode(node->GetParent());
}

bool SensorTreeModel::IsContainer(const wxDataViewItem& item) const
{
    Node* node = GetNodeFromItem(item);
    return node && !node->IsLeaf();
}

bool SensorTreeModel::HasContainerColumns(const wxDataViewItem& item) const
{
    return true;
}

unsigned int SensorTreeModel::GetChildren(const wxDataViewItem& parent, wxDataViewItemArray& array) const
{
    Node* parentNode = GetNodeFromItem(parent);
    
    if (!parentNode)
    {
        // Root level - return root nodes
        for (const auto& node : m_rootNodes)
        {
            array.Add(CreateItemFromNode(node.get()));
        }
        return m_rootNodes.size();
    }
    else
    {
        // Return children of the specified parent
        const auto& children = parentNode->GetChildren();
        for (const auto& child : children)
        {
            array.Add(CreateItemFromNode(child.get()));
        }
        return children.size();
    }
}

void SensorTreeModel::RefreshData()
{
    Cleared();
}

// Helper methods
Node* SensorTreeModel::GetNodeFromItem(const wxDataViewItem& item) const
{
    return static_cast<Node*>(item.GetID());
}

wxDataViewItem SensorTreeModel::CreateItemFromNode(Node* node) const
{
    return wxDataViewItem(static_cast<void*>(node));
}