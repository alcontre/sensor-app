#pragma once
#include "Node.h"

#include <wx/dataview.h>

#include <memory>
#include <vector>

// Custom data model for the hierarchical sensor tree
class SensorTreeModel : public wxDataViewModel
{
 public:
   SensorTreeModel();
   virtual ~SensorTreeModel();

   // Data management
   Node *AddRootNode(std::unique_ptr<Node> node);
   void ClearAll();

   // Add data samples and build tree structure
   void AddDataSample(const std::vector<std::string> &path, const DataValue &value);
   void AddDataSample(const SensorData &data);

   // Filtering support
   void SetFilter(const wxString &filterText);
   const wxString &GetFilter() const { return m_filter; }

   // wxDataViewModel interface
   virtual unsigned int GetColumnCount() const override;
   virtual wxString GetColumnType(unsigned int col) const override;

   virtual void GetValue(wxVariant &variant, const wxDataViewItem &item, unsigned int col) const override;
   virtual bool SetValue(const wxVariant &variant, const wxDataViewItem &item, unsigned int col) override;
   virtual bool GetAttr(const wxDataViewItem &item, unsigned int col, wxDataViewItemAttr &attr) const override;

   virtual wxDataViewItem GetParent(const wxDataViewItem &item) const override;
   virtual bool IsContainer(const wxDataViewItem &item) const override;
   virtual bool HasContainerColumns(const wxDataViewItem &item) const override;
   virtual unsigned int GetChildren(const wxDataViewItem &parent, wxDataViewItemArray &array) const override;

   // Helper methods
   Node *FindOrCreatePath(const std::vector<std::string> &path, bool &structureChanged);
   void RefreshElapsedTimes();

   // Column definitions
   enum Column
   {
      COL_NAME = 0,
      COL_VALUE,
      COL_ELAPSED,
      COL_COUNT
   };

 private:
   std::vector<std::unique_ptr<Node>> m_rootNodes;
   wxString m_filter;
   wxString m_filterLower;

   // Helper methods for item management
   Node *GetNodeFromItem(const wxDataViewItem &item) const;
   wxDataViewItem CreateItemFromNode(Node *node) const;
   bool IsNodeVisible(const Node *node) const;
   bool NodeMatchesFilter(const Node *node) const;
   bool NodeMatchesHighlightFilter(const Node *node) const;
   bool HasVisibleChildren(const Node *node) const;
};