#pragma once
#include "Node.h"
#include <wx/dataview.h>
#include <vector>
#include <memory>

// Custom data model for the hierarchical sensor tree
class SensorTreeModel : public wxDataViewModel
{
public:
    SensorTreeModel();
    virtual ~SensorTreeModel();
    
    // Data management
    void SetRootNodes(const std::vector<std::shared_ptr<Node>>& nodes);
    void AddRootNode(std::shared_ptr<Node> node);
    void ClearAll();
    
    // Add data samples and build tree structure
    void AddDataSample(const std::vector<std::string>& path, const DataValue& value);
    void AddDataSample(std::shared_ptr<SensorData> data);
    
    // wxDataViewModel interface
    virtual unsigned int GetColumnCount() const override;
    virtual wxString GetColumnType(unsigned int col) const override;
    
    virtual void GetValue(wxVariant& variant, const wxDataViewItem& item, unsigned int col) const override;
    virtual bool SetValue(const wxVariant& variant, const wxDataViewItem& item, unsigned int col) override;
    
    virtual wxDataViewItem GetParent(const wxDataViewItem& item) const override;
    virtual bool IsContainer(const wxDataViewItem& item) const override;
    virtual bool HasContainerColumns(const wxDataViewItem& item) const override;
    virtual unsigned int GetChildren(const wxDataViewItem& parent, wxDataViewItemArray& array) const override;
    
    // Helper methods
    void RefreshData();
    std::shared_ptr<Node> FindOrCreatePath(const std::vector<std::string>& path);
    
    // Column definitions
    enum Column
    {
        COL_NAME = 0,
        COL_VALUE,
        COL_TYPE,
        COL_COUNT
    };

private:
    std::vector<std::shared_ptr<Node>> m_rootNodes;
    
    // Helper methods for item management
    Node* GetNodeFromItem(const wxDataViewItem& item) const;
    wxDataViewItem CreateItemFromNode(Node* node) const;
};