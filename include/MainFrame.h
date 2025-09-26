#pragma once
#include "SensorTreeModel.h"

#include "SensorDataEvent.h"
#include "SensorDataGenerator.h"
#include "SensorDataJsonWriter.h"

#include <wx/dataview.h>
#include <wx/event.h>
#include <wx/timer.h>
#include <wx/wx.h>

#include <atomic>
#include <memory>
#include <vector>

class MainFrame : public wxFrame
{
 public:
   MainFrame();

 private:
   void OnExit(wxCommandEvent &event);
   void OnAbout(wxCommandEvent &event);
   void OnToggleDataGenerator(wxCommandEvent &event);

   void CreateMenuBar();
   void SetupStatusBar();
   void CreateSensorTreeView();

   enum
   {
      ID_Hello = 1,
      ID_AgeTimer,

      // Menu bar
      ID_ExpandAll,
      ID_CollapseAll,
      ID_ToggleDataGen,

      // Context menu entries
      ID_ExpandAllHere,
      ID_CollapseChildrenHere
   };

   // UI components
   wxDataViewCtrl *m_treeCtrl;
   std::shared_ptr<SensorTreeModel> m_treeModel;
   wxTimer m_ageTimer;
   std::atomic<bool> m_generationActive;
   SensorDataGenerator *m_dataThread;
   uint64_t m_samplesReceived;
   std::unique_ptr<SensorDataJsonWriter> m_dataRecorder;
   // Track the item for which a context menu is opened
   wxDataViewItem m_contextItem;

   // Event binding setup
   void BindEvents();
   void OnClose(wxCloseEvent &event);
   void OnAgeTimer(wxTimerEvent &event);
   void OnSensorData(wxCommandEvent &event);
   void OnExpandAll(wxCommandEvent &event);
   void OnItemActivated(wxDataViewEvent &event);
   void OnItemContextMenu(wxDataViewEvent &event);
   void OnExpandAllHere(wxCommandEvent &event);
   void OnCollapseChildrenHere(wxCommandEvent &event);
   void OnCollapseAll(wxCommandEvent &event);
   void StartDataGeneration();
   void StopDataGeneration();
};