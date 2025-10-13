#pragma once
#include "Node.h"

#include <wx/dataview.h>

#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

// Custom data model for the hierarchical sensor tree
class SensorTreeModel : public wxDataViewModel
{
 public:
   SensorTreeModel();
   virtual ~SensorTreeModel();

   // Data management
   // Add data samples and build tree structure
   void AddDataSample(const std::vector<std::string> &path, const DataValue &value,
       std::optional<DataValue> lowerThreshold = std::nullopt,
       std::optional<DataValue> upperThreshold = std::nullopt,
       bool failed                             = false);
   void AddDataSample(const SensorData &data);

   // Filtering support
   void SetFilter(const wxString &filterText);
   const wxString &GetFilter() const { return m_filter; }
   void SetShowFailuresOnly(bool showFailuresOnly);
   bool IsShowingFailuresOnly() const { return m_showFailuresOnly; }
   bool IsNodeVisible(const Node *node) const;

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
   struct CreatedEdge
   {
      Node *parent;
      Node *child;
      bool parentWasLeaf;
   };

   Node *FindOrCreatePath(const std::vector<std::string> &path, bool &structureChanged, std::vector<CreatedEdge> &createdEdges);
   void RefreshElapsedTimes();

   // Column definitions
   enum Column
   {
      COL_NAME = 0,
      COL_VALUE,
      COL_LOWER_THRESHOLD,
      COL_UPPER_THRESHOLD,
      COL_ELAPSED,
      COL_STATUS,
      COL_COUNT
   };

 private:
   std::vector<std::unique_ptr<Node>> m_rootNodes;
   wxString m_filter;
   wxString m_filterLower;
   bool m_showFailuresOnly = false;
   std::unordered_set<const Node *> m_visibleNodes;

   // Helper methods for item management
   Node *GetNodeFromItem(const wxDataViewItem &item) const;
   wxDataViewItem CreateItemFromNode(Node *node) const;
   bool ShouldNodeBeVisible(const Node *node) const;
   bool NodeMatchesFilter(const Node *node) const;
   bool NodeMatchesHighlightFilter(const Node *node) const;
   bool HasVisibleChildren(const Node *node) const;
   void CollectVisibleNodes(Node *node, std::vector<Node *> &out) const;
   void UpdateVisibility(Node *valueChanged, const std::vector<Node *> &forceRefreshParents, bool refreshAllVisible);
};