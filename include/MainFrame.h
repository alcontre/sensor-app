#pragma once
#include "SensorTreeModel.h"

#include "SensorDataJsonWriter.h"

#include <wx/dataview.h>
#include <wx/event.h>
#include <wx/timer.h>
#include <wx/wx.h>

#include <atomic>
#include <memory>
#include <string>
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

   // Controls
   ID_RotateLog
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
   void OnCollapseAll(wxCommandEvent &event);
   void OnItemExpanded(wxDataViewEvent &event);
   void OnItemCollapsed(wxDataViewEvent &event);
   void OnRotateLog(wxCommandEvent &event);
   void StartDataTestGeneration();
   void StopDataTestGeneration();
   void RestoreExpansionState();
   void PruneExpansionSubtree(Node *node, bool includeRoot);
   void RotateLogFile(const wxString &reason = wxString());

   std::unordered_set<const Node *> m_expandedNodes;
};