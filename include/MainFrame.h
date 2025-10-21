#pragma once
#include "PlotManager.h"
#include "SensorTreeModel.h"

#include "SensorDataJsonWriter.h"

#include <wx/dataview.h>
#include <wx/event.h>
#include <wx/timer.h>
#include <wx/wx.h>

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

class SensorDataGenerator;
class SensorDataTestGenerator;

enum
{
   ID_Hello = 1,
   ID_AgeTimer,

   // Menu bar
   ID_ExpandAll,
   ID_CollapseAll,
   ID_ToggleDataGen,

   // Connection
   ID_ConnectYes,
   ID_ConnectNo,
   ID_NewMessage,

   // Context menu entries
   ID_ExpandAllHere,
   ID_CollapseChildrenHere,
   ID_SendToNewPlot,

   // Controls
   ID_RotateLog,
   ID_ClearTree,

  ID_SavePlotConfig,
  ID_LoadPlotConfig,

   // Command helpers
   ID_FocusFilter
};

class MainFrame : public wxFrame
{
 public:
   MainFrame();

 private:
   void OnExit(wxCommandEvent &event);
   void OnAbout(wxCommandEvent &event);
   void OnToggleDataGenerator(wxCommandEvent &event);
   void OnFilterTextChanged(wxCommandEvent &event);
   void OnShowFailuresOnly(wxCommandEvent &event);

   void CreateMenuBar();
   void SetupStatusBar();
   void CreateSensorTreeView();
   void UpdateNetworkIndicator(const wxColour &colour, const wxString &tooltip);

   // UI components
   wxDataViewCtrl *m_treeCtrl;
   wxTextCtrl *m_filterCtrl;
   wxPanel *m_networkIndicator;
   wxCheckBox *m_showFailuresOnlyCheck;
   wxButton *m_rotateLogButton;
   wxButton *m_clearTreeButton;
   SensorTreeModel *m_treeModel;
   wxTimer m_ageTimer;
   std::atomic<bool> m_generationActive;
   SensorDataGenerator *m_dataThread;
   SensorDataTestGenerator *m_testDataThread;
   uint64_t m_messagesReceived;
   std::unique_ptr<SensorDataJsonWriter> m_dataRecorder;
   std::string m_currentLogFile;
   bool m_isNetworkConnected;
   // Track the item for which a context menu is opened
   wxDataViewItem m_contextItem;

   // Event binding setup
   void BindEvents();
   void OnClose(wxCloseEvent &event);
   void OnAgeTimer(wxTimerEvent &event);
   void OnSensorData(wxCommandEvent &event);
   void OnConnectionStatus(wxThreadEvent &event);
   void OnNewMessage(wxThreadEvent &event);
   void OnExpandAll(wxCommandEvent &event);
   void OnItemActivated(wxDataViewEvent &event);
   void OnItemContextMenu(wxDataViewEvent &event);
   void OnExpandAllHere(wxCommandEvent &event);
   void OnCollapseChildrenHere(wxCommandEvent &event);
   void OnSendToNewPlot(wxCommandEvent &event);
   void OnSendToExistingPlot(wxCommandEvent &event);
   void OnCollapseAll(wxCommandEvent &event);
   void OnItemExpanded(wxDataViewEvent &event);
   void OnItemCollapsed(wxDataViewEvent &event);
   void OnRotateLog(wxCommandEvent &event);
   void OnClearTree(wxCommandEvent &event);
  void OnSavePlotConfig(wxCommandEvent &event);
  void OnLoadPlotConfig(wxCommandEvent &event);
   void OnFocusFilter(wxCommandEvent &event);
   void OnFilterEnter(wxCommandEvent &event);
   void StartDataTestGeneration();
   void StopDataTestGeneration();
   void RestoreExpansionState();
   void PruneExpansionSubtree(Node *node, bool includeRoot);
   void PopulatePlotMenu(wxMenu &menu);
   void ClearDynamicPlotMenuItems();
   std::vector<Node *> CollectPlotEligibleNodes(wxString &messageOut) const;
   void RotateLogFile(const wxString &reason = wxString());
   void CloseLogFile(const wxString &reason = wxString());
   void SetupAccelerators();

   std::unordered_set<const Node *> m_expandedNodes;
   std::unique_ptr<PlotManager> m_plotManager;
   std::unordered_map<int, wxString> m_plotMenuIdToName;
};