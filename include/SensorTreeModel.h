#pragma once
#include "Node.h"

#include <wx/dataview.h>

#include <functional>
#include <memory>
#include <vector>

// Custom data model for the hierarchical sensor tree
class SensorTreeModel : public wxDataViewModel
{
 public:
   SensorTreeModel();
   ~SensorTreeModel() override;

   void AddDataSample(const std::vector<std::string> &path, const DataValue &value,
       SensorThresholds thresholds = {},
       SensorAlarmState alarmState = SensorAlarmState::Ok);
   void SetLiveDataMode(bool isLiveData);
   bool IsLiveDataMode() const { return m_isLiveDataMode; }

   void SetFilter(const wxString &filterText);
   const wxString &GetFilter() const { return m_filter; }
   void SetShowAlarmedOnly(bool showAlarmedOnly);
   bool IsShowingAlarmedOnly() const { return m_showAlarmedOnly; }
   bool IsNodeVisible(const Node *node) const;
   void SetExpansionQuery(std::function<bool(const Node *)> query);

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
   void Clear();

   Node *FindNodeByPath(const std::vector<std::string> &path) const;

   enum Column
   {
      COL_NAME = 0,
      COL_VALUE,
      COL_LOWER_CRITICAL_THRESHOLD,
      COL_LOWER_NON_CRITICAL_THRESHOLD,
      COL_UPPER_NON_CRITICAL_THRESHOLD,
      COL_UPPER_CRITICAL_THRESHOLD,
      COL_ELAPSED,
      COL_UPDATE_COUNT,
      COL_COUNT
   };

   struct AlarmSummary
   {
      size_t warningCount = 0;
      size_t failureCount = 0;
   };

 private:
   struct CreatedEdge
   {
      Node *parent;
      Node *child;
      bool parentWasLeaf;
   };

   struct VisibleSubtreeState
   {
      bool isVisible = false;
      AlarmSummary alarmSummary;
   };

   Node *FindOrCreatePath(const std::vector<std::string> &path, bool &structureChanged, std::vector<CreatedEdge> &createdEdges);
   std::vector<Node *> BuildPath(Node *node) const;

   std::vector<std::unique_ptr<Node>> m_rootNodes;
   wxString m_filter;
   wxString m_filterLower;
   bool m_showAlarmedOnly = false;
   bool m_isLiveDataMode  = true;

   Node *GetNodeFromItem(const wxDataViewItem &item) const;
   wxDataViewItem CreateItemFromNode(Node *node) const;
   bool NodeMatchesFilter(const Node *node) const;
   bool NodeNameMatchesFilter(const Node *node) const;
   bool NodeMatchesAlarmFilter(const Node *node) const;
   VisibleSubtreeState EvaluateVisibleSubtree(const Node *node) const;
   bool HasVisibleChildren(const Node *node) const;
   AlarmSummary CountAlarmedDescendants(const Node *node) const;

   std::function<bool(const Node *)> m_isNodeExpanded;
};