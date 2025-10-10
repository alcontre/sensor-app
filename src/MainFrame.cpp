#include "MainFrame.h"

#include "SensorDataEvent.h"
#include "SensorDataGenerator.h"
#include "SensorDataTestGenerator.h"
#include "SensorTreeModel.h"

#include <functional>
#include <vector>

MainFrame::MainFrame() :
    wxFrame(nullptr, wxID_ANY, "Sensor Tree Viewer",
        wxDefaultPosition, wxSize(800, 600)),
    m_treeCtrl(nullptr),
    m_filterCtrl(nullptr),
    m_showFailuresOnlyCheck(nullptr),
    m_networkIndicator(nullptr),
    m_ageTimer(this, ID_AgeTimer),
    m_generationActive(false),
    m_dataThread(nullptr),
    m_testDataThread(nullptr),
    m_messagesReceived(0),
    m_treeModel(nullptr)
{
   CreateMenuBar();
   SetupStatusBar();
   CreateSensorTreeView();
   BindEvents();

   m_dataRecorder = std::make_unique<SensorDataJsonWriter>();

   m_dataThread = new SensorDataGenerator(this);
   if (m_dataThread->Run() != wxTHREAD_NO_ERROR) {
      delete m_dataThread;
      m_dataThread = nullptr;
   }

   m_testDataThread = new SensorDataTestGenerator(m_generationActive, this);
   if (m_testDataThread->Run() != wxTHREAD_NO_ERROR) {
      delete m_testDataThread;
      m_testDataThread = nullptr;
   }

   // Start automatic data generation (will run indefinitely)
   // StartDataTestGeneration();
   m_ageTimer.Start(250);

   wxString status = "";
   if (m_dataRecorder->IsOpen()) {
      status += " Logging enabled.";
   } else {
      status += " Logging disabled (unable to open log file).";
   }
   // Put welcome/status messages on the right status field
   SetStatusText(status, 1);
}

void MainFrame::CreateMenuBar()
{
   wxMenu *menuFile = new wxMenu;
   menuFile->Append(ID_Hello, "&About...",
       "Show information about this application");
   // Toggle automatic data generator
   menuFile->AppendCheckItem(ID_ToggleDataGen, "&Toggle Data Generator",
       "Enable or disable automatic sensor data generation");
   menuFile->AppendSeparator();
   menuFile->Append(wxID_EXIT);

   wxMenuBar *menuBar = new wxMenuBar;
   menuBar->Append(menuFile, "&File");

   // View menu: expand/collapse helpers
   wxMenu *menuView = new wxMenu;
   menuView->Append(ID_ExpandAll, "&Expand All\tCtrl-E", "Expand all nodes in the tree view");
   menuView->Append(ID_CollapseAll, "&Collapse All\tCtrl-Shift-E", "Collapse all nodes in the tree view");
   menuBar->Append(menuView, "&View");

   SetMenuBar(menuBar);
}

void MainFrame::SetupStatusBar()
{
   // Create a two-field status bar; connection info and msg counts
   CreateStatusBar(2);

   // Ensure left is blank and initialize right with zero messages received
   SetStatusText("", 0);
   SetStatusText(wxString::Format("Messages received: %zu", (unsigned long long)m_messagesReceived), 1);
}

void MainFrame::OnExit(wxCommandEvent &event)
{
   Close(true);
}

void MainFrame::OnAbout(wxCommandEvent &event)
{
   wxMessageBox("Sensor Tree Viewer\n"
                "A hierarchical sensor data display application\n"
                "Supports arbitrary hierarchical data structures",
       "About Sensor Tree Viewer", wxOK | wxICON_INFORMATION);
}

// Removed manual test data generation; data is created by the auto-generator

// Start/stop handlers removed; generation starts automatically

void MainFrame::CreateSensorTreeView()
{
   // Create main panel
   wxPanel *panel = new wxPanel(this, wxID_ANY);

   // Create the tree model
   m_treeModel = new SensorTreeModel();

   // Create the data view control
   m_treeCtrl = new wxDataViewCtrl(panel, wxID_ANY,
       wxDefaultPosition, wxDefaultSize,
       wxDV_MULTIPLE | wxDV_ROW_LINES | wxDV_HORIZ_RULES);

   // Associate the model with the control
   m_treeCtrl->AssociateModel(m_treeModel);
   m_treeModel->DecRef();

   // Add columns
   m_treeCtrl->AppendTextColumn("Name", SensorTreeModel::COL_NAME, wxDATAVIEW_CELL_INERT, 200);
   m_treeCtrl->AppendTextColumn("Value", SensorTreeModel::COL_VALUE, wxDATAVIEW_CELL_INERT, 120, wxALIGN_CENTER); // value display
   m_treeCtrl->AppendTextColumn("Lower Threshold", SensorTreeModel::COL_LOWER_THRESHOLD, wxDATAVIEW_CELL_INERT, 130, wxALIGN_CENTER);
   m_treeCtrl->AppendTextColumn("Upper Threshold", SensorTreeModel::COL_UPPER_THRESHOLD, wxDATAVIEW_CELL_INERT, 130, wxALIGN_CENTER);
   m_treeCtrl->AppendTextColumn("Last Updated", SensorTreeModel::COL_ELAPSED, wxDATAVIEW_CELL_INERT, 100, wxALIGN_CENTER);
   m_treeCtrl->AppendTextColumn("Status", SensorTreeModel::COL_STATUS, wxDATAVIEW_CELL_INERT, 90, wxALIGN_CENTER);

   // Layout
   wxBoxSizer *sizer       = new wxBoxSizer(wxVERTICAL);
   wxBoxSizer *filterSizer = new wxBoxSizer(wxHORIZONTAL);

   // Add a connection indicator
   m_networkIndicator = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxSize(20, 20), wxSIMPLE_BORDER);

   // Determine an aesthetically pleasing size based on default button height
   const auto default_btn_size = wxButton::GetDefaultSize(this);
   const wxSize indicator_size(default_btn_size.y, default_btn_size.y);
   m_networkIndicator->SetMinSize(indicator_size);
   m_networkIndicator->SetMaxSize(indicator_size);

   UpdateNetworkIndicator(*wxYELLOW, "Network idle");

   // TODO - click connect indicator to reset connection (not implemented)

   m_showFailuresOnlyCheck = new wxCheckBox(panel, wxID_ANY, "Show failures only");
   m_showFailuresOnlyCheck->SetToolTip("Only display sensors currently in a failed state");

   // Add sensor filter text box
   m_filterCtrl = new wxTextCtrl(panel, wxID_ANY);
   m_filterCtrl->SetHint("Type to filter sensors...");

   filterSizer->Add(m_networkIndicator, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
   filterSizer->Add(m_showFailuresOnlyCheck, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
   filterSizer->Add(m_filterCtrl, 1, wxEXPAND);

   sizer->Add(filterSizer, 0, wxEXPAND | wxALL, 5);
   sizer->Add(m_treeCtrl, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
   panel->SetSizer(sizer);
}

// PopulateTestData removed: tree will be populated dynamically by incoming samples

void MainFrame::BindEvents()
{
   // Bind menu events using modern Bind() syntax
   Bind(wxEVT_MENU, &MainFrame::OnAbout, this, ID_Hello);
   Bind(wxEVT_MENU, &MainFrame::OnToggleDataGenerator, this, ID_ToggleDataGen);
   Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
   // Bind close event to ensure model is disassociated before destruction
   Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
   Bind(wxEVT_TIMER, &MainFrame::OnAgeTimer, this, ID_AgeTimer);
   Bind(wxEVT_SENSOR_DATA_SAMPLE, &MainFrame::OnSensorData, this);
   Bind(wxEVT_MENU, &MainFrame::OnExpandAll, this, ID_ExpandAll);
   Bind(wxEVT_MENU, &MainFrame::OnCollapseAll, this, ID_CollapseAll);
   // Toggle expand/collapse on double-click (item activated)
   Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &MainFrame::OnItemActivated, this);
   // Context menu for items
   Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &MainFrame::OnItemContextMenu, this);
   Bind(wxEVT_MENU, &MainFrame::OnExpandAllHere, this, ID_ExpandAllHere);
   Bind(wxEVT_MENU, &MainFrame::OnCollapseChildrenHere, this, ID_CollapseChildrenHere);

   m_filterCtrl->Bind(wxEVT_TEXT, &MainFrame::OnFilterTextChanged, this);

   m_showFailuresOnlyCheck->Bind(wxEVT_CHECKBOX, &MainFrame::OnShowFailuresOnly, this);

   Bind(wxEVT_THREAD, &MainFrame::OnConnectionStatus, this, ID_ConnectYes);
   Bind(wxEVT_THREAD, &MainFrame::OnConnectionStatus, this, ID_ConnectNo);
   Bind(wxEVT_THREAD, &MainFrame::OnNewMessage, this, ID_NewMessage);
}

void MainFrame::OnAgeTimer(wxTimerEvent &event)
{
   if (m_treeModel) {
      m_treeModel->RefreshElapsedTimes();
   }
}

void MainFrame::OnSensorData(wxCommandEvent &event)
{
   auto *sampleEvent = dynamic_cast<SensorDataEvent *>(&event);
   if (!sampleEvent || !m_treeModel)
      return;

   m_treeModel->AddDataSample(sampleEvent->GetPath(), sampleEvent->GetValue(),
       sampleEvent->GetLowerThreshold(), sampleEvent->GetUpperThreshold(), sampleEvent->IsFailed());
   if (m_dataRecorder)
      m_dataRecorder->RecordSample(sampleEvent->GetPath(), sampleEvent->GetValue(),
          sampleEvent->GetLowerThreshold(), sampleEvent->GetUpperThreshold(), sampleEvent->IsFailed());
}

// Recursively expand all descendants of a given item
static void ExpandDescendants(wxDataViewCtrl *ctrl, const wxDataViewItem &parent, const SensorTreeModel *model)
{
   wxDataViewItemArray children;
   model->GetChildren(parent, children);
   for (const wxDataViewItem &child : children) {
      ctrl->Expand(child);
      ExpandDescendants(ctrl, child, model);
   }
}

// Recursively collapse all descendants of a given item
static void CollapseDescendants(wxDataViewCtrl *ctrl, const wxDataViewItem &parent, const SensorTreeModel *model)
{
   wxDataViewItemArray children;
   model->GetChildren(parent, children);
   for (const wxDataViewItem &child : children) {
      CollapseDescendants(ctrl, child, model);
      ctrl->Collapse(child);
   }
}

void MainFrame::OnFilterTextChanged(wxCommandEvent &event)
{
   if (!m_treeModel || !m_treeCtrl)
      return;

   std::vector<Node *> expandedNodes;
   std::function<void(const wxDataViewItem &)> collectExpanded;
   collectExpanded = [&](const wxDataViewItem &parent) {
      wxDataViewItemArray children;
      m_treeModel->GetChildren(parent, children);
      for (const wxDataViewItem &child : children) {
         if (m_treeCtrl->IsExpanded(child)) {
            expandedNodes.push_back(static_cast<Node *>(child.GetID()));
         }
         collectExpanded(child);
      }
   };
   collectExpanded(wxDataViewItem(nullptr));

   wxString filterText = event.GetString();
   m_treeModel->SetFilter(filterText);

   if (!m_treeCtrl)
      return;

   m_treeCtrl->Freeze();
   for (Node *node : expandedNodes) {
      if (!node)
         continue;
      wxDataViewItem item(static_cast<void *>(node));
      m_treeCtrl->Expand(item);
   }
   m_treeCtrl->Thaw();
}

void MainFrame::OnShowFailuresOnly(wxCommandEvent &event)
{
   if (!m_treeModel || !m_treeCtrl)
      return;

   std::vector<Node *> expandedNodes;
   std::function<void(const wxDataViewItem &)> collectExpanded;
   collectExpanded = [&](const wxDataViewItem &parent) {
      wxDataViewItemArray children;
      m_treeModel->GetChildren(parent, children);
      for (const wxDataViewItem &child : children) {
         if (m_treeCtrl->IsExpanded(child)) {
            expandedNodes.push_back(static_cast<Node *>(child.GetID()));
         }
         collectExpanded(child);
      }
   };
   collectExpanded(wxDataViewItem(nullptr));

   m_treeModel->SetShowFailuresOnly(event.IsChecked());

   if (!m_treeCtrl)
      return;

   m_treeCtrl->Freeze();
   for (Node *node : expandedNodes) {
      if (!node)
         continue;
      wxDataViewItem item(static_cast<void *>(node));
      m_treeCtrl->Expand(item);
   }
   m_treeCtrl->Thaw();
}

void MainFrame::OnExpandAll(wxCommandEvent &event)
{
   if (!m_treeCtrl || !m_treeModel)
      return;

   // Start from the invisible root
   wxDataViewItem root = wxDataViewItem(NULL);
   ExpandDescendants(m_treeCtrl, root, m_treeModel);
}

void MainFrame::OnCollapseAll(wxCommandEvent &event)
{
   if (!m_treeCtrl || !m_treeModel)
      return;

   wxDataViewItem root = wxDataViewItem(NULL);
   CollapseDescendants(m_treeCtrl, root, m_treeModel);
}

void MainFrame::OnItemActivated(wxDataViewEvent &event)
{
   if (!m_treeCtrl)
      return;

   wxDataViewItem item = event.GetItem();
   if (!item.IsOk())
      return;

   if (m_treeCtrl->IsExpanded(item))
      m_treeCtrl->Collapse(item);
   else
      m_treeCtrl->Expand(item);
}

void MainFrame::OnItemContextMenu(wxDataViewEvent &event)
{
   m_contextItem = event.GetItem();
   wxMenu menu;
   menu.Append(ID_ExpandAllHere, "Expand All");
   menu.Append(ID_CollapseChildrenHere, "Collapse Children");
   PopupMenu(&menu);
}

void MainFrame::OnExpandAllHere(wxCommandEvent &event)
{
   if (!m_treeCtrl || !m_treeModel)
      return;

   wxDataViewItem start = m_contextItem.IsOk() ? m_contextItem : wxDataViewItem(NULL);
   ExpandDescendants(m_treeCtrl, start, m_treeModel);
}

void MainFrame::OnCollapseChildrenHere(wxCommandEvent &event)
{
   if (!m_treeCtrl || !m_treeModel)
      return;

   wxDataViewItem start = m_contextItem.IsOk() ? m_contextItem : wxDataViewItem(NULL);
   CollapseDescendants(m_treeCtrl, start, m_treeModel);
   // Also collapse the starting item itself if it's a valid node
   if (m_contextItem.IsOk())
      m_treeCtrl->Collapse(m_contextItem);
}

void MainFrame::OnConnectionStatus(wxThreadEvent &event)
{
   switch (event.GetId()) {
      case ID_ConnectYes: {
         UpdateNetworkIndicator(*wxGREEN, "Network connected");
         break;
      }
      case ID_ConnectNo: {
         UpdateNetworkIndicator(*wxYELLOW, "Network idle");
         break;
      }
      default:
         break;
   }
}

void MainFrame::OnNewMessage(wxThreadEvent &event)
{
   (void)event; // no payload expected
   ++m_messagesReceived;
   SetStatusText(wxString::Format("Messages received: %zu", (unsigned long long)m_messagesReceived), 1);
}

void MainFrame::UpdateNetworkIndicator(const wxColour &colour, const wxString &tooltip)
{
   m_networkIndicator->SetBackgroundColour(colour);
   m_networkIndicator->SetToolTip(tooltip);
   m_networkIndicator->Refresh();
}

void MainFrame::StartDataTestGeneration()
{
   if (m_generationActive.load())
      return;

   m_generationActive = true;
   SetStatusText("Auto data generation started", 1);
   // Check the menu item if present on this frame
   if (GetMenuBar()) {
      wxMenuItem *mi = GetMenuBar()->FindItem(ID_ToggleDataGen);
      if (mi)
         mi->Check(true);
   }
}

void MainFrame::StopDataTestGeneration()
{
   if (!m_generationActive.load())
      return;

   m_generationActive = false;
   SetStatusText("Auto data generation stopped", 1);
   // Uncheck the menu item if present on this frame
   if (GetMenuBar()) {
      wxMenuItem *mi = GetMenuBar()->FindItem(ID_ToggleDataGen);
      if (mi)
         mi->Check(false);
   }
}

void MainFrame::OnToggleDataGenerator(wxCommandEvent &event)
{
   // Toggle based on current atomic flag
   if (m_generationActive.load())
      StopDataTestGeneration();
   else
      StartDataTestGeneration();
}

void MainFrame::OnClose(wxCloseEvent &event)
{
   StopDataTestGeneration();
   if (m_ageTimer.IsRunning()) {
      m_ageTimer.Stop();
   }
   // Ensure the data view control disassociates the model before it is destroyed.
   if (m_treeCtrl && m_treeModel) {
      // Disassociate model to prevent wxDataViewCtrl from trying to remove notifier
      m_treeCtrl->AssociateModel(nullptr);
      m_treeModel = nullptr;
   }

   m_dataRecorder.reset();

   // Proceed with default close handling
   event.Skip();
}