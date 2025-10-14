#pragma once
#include "Node.h"

#include <wx/dataview.h>

#include <memory>
#include <optional>
#include <vector>

// Custom data model for the hierarchical sensor tree
class SensorTreeModel : public wxDataViewModel
{
 public:
   SensorTreeModel();
   ~SensorTreeModel() override;

   void AddDataSample(const std::vector<std::string> &path, const DataValue &value,
       std::optional<DataValue> lowerThreshold = std::nullopt,
       std::optional<DataValue> upperThreshold = std::nullopt,
       bool failed                             = false);
   void AddDataSample(const SensorData &data);

   void SetFilter(const wxString &filterText);
   const wxString &GetFilter() const { return m_filter; }
   void SetShowFailuresOnly(bool showFailuresOnly);
   bool IsShowingFailuresOnly() const { return m_showFailuresOnly; }
   bool IsNodeVisible(const Node *node) const;

   unsigned int GetColumnCount() const override;
   wxString GetColumnType(unsigned int col) const override;

   void GetValue(wxVariant &variant, const wxDataViewItem &item, unsigned int col) const override;
   bool SetValue(const wxVariant &variant, const wxDataViewItem &item, unsigned int col) override;
   bool GetAttr(const wxDataViewItem &item, unsigned int col, wxDataViewItemAttr &attr) const override;

   wxDataViewItem GetParent(const wxDataViewItem &item) const override;
   bool IsContainer(const wxDataViewItem &item) const override;
   bool HasContainerColumns(const wxDataViewItem &item) const override;
   unsigned int GetChildren(const wxDataViewItem &parent, wxDataViewItemArray &array) const override;

   void RefreshElapsedTimes();

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
   struct CreatedEdge
   {
      Node *parent;
      Node *child;
      bool parentWasLeaf;
   };

   Node *FindOrCreatePath(const std::vector<std::string> &path, bool &structureChanged, std::vector<CreatedEdge> &createdEdges);
   std::vector<Node *> BuildPath(Node *node) const;

   std::vector<std::unique_ptr<Node>> m_rootNodes;
   wxString m_filter;
   wxString m_filterLower;
   bool m_showFailuresOnly = false;

   Node *GetNodeFromItem(const wxDataViewItem &item) const;
   wxDataViewItem CreateItemFromNode(Node *node) const;
   bool ShouldNodeBeVisible(const Node *node) const;
   bool NodeMatchesFilter(const Node *node) const;
   bool NodeNameMatchesFilter(const Node *node) const;
   bool HasVisibleChildren(const Node *node) const;
};