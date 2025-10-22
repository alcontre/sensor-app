#include "MainFrame.h"

#include "SensorDataEvent.h"
#include "SensorDataGenerator.h"
#include "SensorDataTestGenerator.h"
#include "SensorTreeModel.h"

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <wx/accel.h>
#include <wx/fileconf.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/window.h>

namespace {
std::string ToUtf8(const wxString &value)
{
   const wxScopedCharBuffer buffer = value.ToUTF8();
   if (!buffer)
      return std::string();
   return std::string(buffer.data(), buffer.length());
}

constexpr int STATUS_FIELD_NET_STATUS    = 0;
constexpr int STATUS_FIELD_LOG_INFO      = 1;
constexpr int STATUS_FIELD_MESSAGE_COUNT = 2;
constexpr int STATUS_FIELD_COUNT         = 3;
} // namespace

MainFrame::MainFrame() :
    wxFrame(nullptr, wxID_ANY, "Sensor Tree Viewer",
        wxDefaultPosition, wxSize(800, 600)),
    m_treeCtrl(nullptr),
    m_filterCtrl(nullptr),
    m_showFailuresOnlyCheck(nullptr),
    m_networkIndicator(nullptr),
    m_rotateLogButton(nullptr),
    m_clearTreeButton(nullptr),
    m_treeModel(nullptr),
    m_ageTimer(this, ID_AgeTimer),
    m_generationActive(false),
    m_dataThread(nullptr),
    m_testDataThread(nullptr),
    m_messagesReceived(0),
    m_currentLogFile(),
    m_isNetworkConnected(false),
    m_plotManager(nullptr)
{
   CreateMenuBar();
   SetupStatusBar();
   CreateSensorTreeView();
   SetupAccelerators();
   BindEvents();

   m_plotManager = std::make_unique<PlotManager>(this, m_treeModel);

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
   StartDataTestGeneration();
   m_ageTimer.Start(50);
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
   menuFile->Append(ID_RotateLog, "&Rotate Log",
       "Finish the current log file and start a new one");
   menuFile->AppendSeparator();
   menuFile->Append(ID_SavePlotConfig, "&Save Plot Configuration...",
       "Write the open plots and their assigned sensors to a config file");
   menuFile->Append(ID_LoadPlotConfig, "&Load Plot Configuration...",
       "Open plots based on a previously saved configuration");
   menuFile->AppendSeparator();
   menuFile->Append(wxID_EXIT);

   wxMenuBar *menuBar = new wxMenuBar;
   menuBar->Append(menuFile, "&File");

   // View menu: expand/collapse helpers
   wxMenu *menuView = new wxMenu;
   menuView->Append(ID_ExpandAll, "&Expand All\tCtrl-E", "Expand all nodes in the tree view");
   menuView->Append(ID_CollapseAll, "&Collapse All\tCtrl-Shift-E", "Collapse all nodes in the tree view");
   menuView->AppendSeparator();
   menuView->Append(ID_ClearTree, "&Clear Entries", "Remove all sensor data from the tree view");
   menuBar->Append(menuView, "&View");

   SetMenuBar(menuBar);
}

void MainFrame::SetupStatusBar()
{
   // Create a three-field status bar; left reserved, middle log file, right message count
   CreateStatusBar(STATUS_FIELD_COUNT);

   SetStatusText("", STATUS_FIELD_NET_STATUS);
   SetStatusText("Current log: (no active log)", STATUS_FIELD_LOG_INFO);
   SetStatusText(wxString::Format("Messages received: %zu", (unsigned long long)m_messagesReceived), STATUS_FIELD_MESSAGE_COUNT);
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

   // Provide a callback so the model can determine the expansion state of nodes
   m_treeModel->SetExpansionQuery([this](const Node *node) {
      if (!node)
         return false;
      wxDataViewItem item(const_cast<Node *>(node));
      return m_treeCtrl->IsExpanded(item);
   });

   // Add columns
   m_treeCtrl->AppendTextColumn("Name", SensorTreeModel::COL_NAME, wxDATAVIEW_CELL_INERT, 200);
   m_treeCtrl->AppendTextColumn("Value", SensorTreeModel::COL_VALUE, wxDATAVIEW_CELL_INERT, 120, wxALIGN_CENTER); // value display
   m_treeCtrl->AppendTextColumn("Lower Threshold", SensorTreeModel::COL_LOWER_THRESHOLD, wxDATAVIEW_CELL_INERT, 130, wxALIGN_CENTER);
   m_treeCtrl->AppendTextColumn("Upper Threshold", SensorTreeModel::COL_UPPER_THRESHOLD, wxDATAVIEW_CELL_INERT, 130, wxALIGN_CENTER);
   m_treeCtrl->AppendTextColumn("Last Updated", SensorTreeModel::COL_ELAPSED, wxDATAVIEW_CELL_INERT, 100, wxALIGN_CENTER);
   m_treeCtrl->AppendTextColumn("Updates", SensorTreeModel::COL_UPDATE_COUNT, wxDATAVIEW_CELL_INERT, 90, wxALIGN_CENTER);

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

   m_rotateLogButton = new wxButton(panel, ID_RotateLog, "&Rotate Log");
   m_rotateLogButton->SetToolTip("Finish the current log file and start a new one");

   m_clearTreeButton = new wxButton(panel, ID_ClearTree, "&Clear");
   m_clearTreeButton->SetToolTip("Remove all sensor data from the tree view");

   m_showFailuresOnlyCheck = new wxCheckBox(panel, wxID_ANY, "&Show failures only");
   m_showFailuresOnlyCheck->SetToolTip("Only display sensors currently in a failed state");

   // Add sensor filter text box
   // Process Enter locally so the key does not bubble to menu accelerators (e.g. About on Windows).
   m_filterCtrl = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
   m_filterCtrl->SetHint("Type to filter sensors... (Ctrl+F)");

   filterSizer->Add(m_networkIndicator, 0, wxALIGN_CENTER_VERTICAL);
   filterSizer->Add(m_rotateLogButton, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
   filterSizer->Add(m_clearTreeButton, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
   filterSizer->Add(m_showFailuresOnlyCheck, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
   filterSizer->Add(m_filterCtrl, 1, wxEXPAND | wxLEFT, 8);

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
   Bind(wxEVT_MENU, &MainFrame::OnRotateLog, this, ID_RotateLog);
   Bind(wxEVT_MENU, &MainFrame::OnSavePlotConfig, this, ID_SavePlotConfig);
   Bind(wxEVT_MENU, &MainFrame::OnLoadPlotConfig, this, ID_LoadPlotConfig);
   Bind(wxEVT_MENU, &MainFrame::OnClearTree, this, ID_ClearTree);
   Bind(wxEVT_MENU, &MainFrame::OnFocusFilter, this, ID_FocusFilter);
   // Toggle expand/collapse on double-click (item activated)
   Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &MainFrame::OnItemActivated, this);
   Bind(wxEVT_DATAVIEW_ITEM_EXPANDED, &MainFrame::OnItemExpanded, this);
   Bind(wxEVT_DATAVIEW_ITEM_COLLAPSED, &MainFrame::OnItemCollapsed, this);
   Bind(wxEVT_BUTTON, &MainFrame::OnRotateLog, this, ID_RotateLog);
   Bind(wxEVT_BUTTON, &MainFrame::OnClearTree, this, ID_ClearTree);
   // Context menu for items
   Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &MainFrame::OnItemContextMenu, this);
   Bind(wxEVT_MENU, &MainFrame::OnExpandAllHere, this, ID_ExpandAllHere);
   Bind(wxEVT_MENU, &MainFrame::OnCollapseChildrenHere, this, ID_CollapseChildrenHere);
   Bind(wxEVT_MENU, &MainFrame::OnSendToNewPlot, this, ID_SendToNewPlot);

   m_filterCtrl->Bind(wxEVT_TEXT, &MainFrame::OnFilterTextChanged, this);
   m_filterCtrl->Bind(wxEVT_TEXT_ENTER, &MainFrame::OnFilterEnter, this);

   m_showFailuresOnlyCheck->Bind(wxEVT_CHECKBOX, &MainFrame::OnShowFailuresOnly, this);

   Bind(wxEVT_THREAD, &MainFrame::OnConnectionStatus, this, ID_ConnectYes);
   Bind(wxEVT_THREAD, &MainFrame::OnConnectionStatus, this, ID_ConnectNo);
   Bind(wxEVT_THREAD, &MainFrame::OnNewMessage, this, ID_NewMessage);
}

void MainFrame::OnAgeTimer(wxTimerEvent &event)
{
   m_treeModel->RefreshElapsedTimes();
}

void MainFrame::OnSensorData(wxCommandEvent &event)
{
   auto *sampleEvent = dynamic_cast<SensorDataEvent *>(&event);
   if (!sampleEvent)
      return;

   m_treeModel->AddDataSample(sampleEvent->GetPath(), sampleEvent->GetValue(),
       sampleEvent->GetLowerThreshold(), sampleEvent->GetUpperThreshold(), sampleEvent->IsFailed());
   if (m_dataRecorder)
      m_dataRecorder->RecordSample(sampleEvent->GetPath(), sampleEvent->GetValue(),
          sampleEvent->GetLowerThreshold(), sampleEvent->GetUpperThreshold(), sampleEvent->IsFailed());

   if (m_showFailuresOnlyCheck->IsChecked()) {
      m_treeCtrl->Freeze();
      RestoreExpansionState();
      m_treeCtrl->Thaw();
   }
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
   wxString filterText = event.GetString();

   m_treeCtrl->Freeze();
   m_treeModel->SetFilter(filterText);
   RestoreExpansionState();
   m_treeCtrl->Thaw();
}

void MainFrame::OnShowFailuresOnly(wxCommandEvent &event)
{
   m_treeCtrl->Freeze();
   m_treeModel->SetShowFailuresOnly(event.IsChecked());
   RestoreExpansionState();
   m_treeCtrl->Thaw();
}

void MainFrame::OnItemExpanded(wxDataViewEvent &event)
{
   Node *node = static_cast<Node *>(event.GetItem().GetID());
   if (node)
      m_expandedNodes.insert(node);

   // Force the view to re-query the model so the failure summary reflects the new expansion state.
   m_treeCtrl->Refresh();
   event.Skip();
}

void MainFrame::OnItemCollapsed(wxDataViewEvent &event)
{
   Node *node = static_cast<Node *>(event.GetItem().GetID());
   if (node)
      PruneExpansionSubtree(node, true);
   // Force the view to re-query the model so the failure summary reflects the new expansion state.
   m_treeCtrl->Refresh();
   event.Skip();
}

void MainFrame::RestoreExpansionState()
{
   std::vector<Node *> nodes;
   nodes.reserve(m_expandedNodes.size());
   for (const Node *nodePtr : m_expandedNodes) {
      if (!nodePtr)
         continue;
      if (!m_treeModel->IsNodeVisible(nodePtr))
         continue;
      nodes.push_back(const_cast<Node *>(nodePtr));
   }

   std::sort(nodes.begin(), nodes.end(),
       [](Node *lhs, Node *rhs) {
          return lhs->GetDepth() < rhs->GetDepth();
       });

   for (Node *node : nodes) {
      wxDataViewItem item(static_cast<void *>(node));
      m_treeCtrl->Expand(item);
   }
}

void MainFrame::PruneExpansionSubtree(Node *node, bool includeRoot)
{
   if (!node)
      return;

   std::vector<Node *> stack;
   stack.push_back(node);

   while (!stack.empty()) {
      Node *current = stack.back();
      stack.pop_back();

      if (current != node || includeRoot) {
         m_expandedNodes.erase(current);
      }

      for (const auto &child : current->GetChildren()) {
         stack.push_back(child.get());
      }
   }
}

void MainFrame::OnExpandAll(wxCommandEvent &event)
{
   // Start from the invisible root
   wxDataViewItem root = wxDataViewItem(NULL);
   ExpandDescendants(m_treeCtrl, root, m_treeModel);

   m_expandedNodes.clear();
   std::function<void(const wxDataViewItem &)> recordExpanded = [&](const wxDataViewItem &parent) {
      wxDataViewItemArray children;
      m_treeModel->GetChildren(parent, children);
      for (const wxDataViewItem &child : children) {
         Node *node = static_cast<Node *>(child.GetID());
         if (node)
            m_expandedNodes.insert(node);
         recordExpanded(child);
      }
   };
   recordExpanded(root);
}

void MainFrame::OnCollapseAll(wxCommandEvent &event)
{
   wxDataViewItem root = wxDataViewItem(NULL);
   CollapseDescendants(m_treeCtrl, root, m_treeModel);
   m_expandedNodes.clear();
}

void MainFrame::OnSendToNewPlot(wxCommandEvent &WXUNUSED(event))
{
   wxString skippedMessage;
   std::vector<Node *> nodes = CollectPlotEligibleNodes(skippedMessage);
   if (nodes.empty()) {
      wxString feedback = skippedMessage.IsEmpty() ? wxString("Select one or more sensors with numeric data to plot.") : skippedMessage;
      wxMessageBox(feedback, "Send to Plot", wxOK | wxICON_INFORMATION, this);
      return;
   }

   // Find next unique plot name
   unsigned long long plot_idx = 0;
   wxString defaultPlotName;
   while (true) {
      defaultPlotName = wxString::Format("Plot %llu", plot_idx);
      if (!m_plotManager->HasPlot(defaultPlotName))
         break;
      ++plot_idx;
   }
   wxTextEntryDialog dialog(this, "Enter a name for the new plot:", "Create Plot");
   dialog.SetValue(defaultPlotName);
   // Automatically select the text for easy replacement
   const wxWindowList &children = dialog.GetChildren();
   for (wxWindowList::compatibility_iterator node = children.GetFirst(); node; node = node->GetNext()) {
      wxWindow *child = node->GetData();
      if (auto *textCtrl = wxDynamicCast(child, wxTextCtrl)) {
         textCtrl->SelectAll();
         break;
      }
   }
   if (dialog.ShowModal() != wxID_OK)
      return;

   wxString plotName = dialog.GetValue();
   plotName.Trim(true);
   plotName.Trim(false);

   if (plotName.IsEmpty()) {
      wxMessageBox("Plot name cannot be empty.", "Create Plot", wxOK | wxICON_WARNING, this);
      return;
   }

   if (m_plotManager->HasPlot(plotName)) {
      wxMessageBox("A plot with that name already exists. Choose another name.", "Create Plot", wxOK | wxICON_WARNING, this);
      return;
   }

   m_plotManager->CreatePlot(plotName, nodes);

   if (!skippedMessage.IsEmpty())
      wxLogMessage(skippedMessage);
}

void MainFrame::OnSendToExistingPlot(wxCommandEvent &event)
{
   auto it = m_plotMenuIdToName.find(event.GetId());
   if (it == m_plotMenuIdToName.end())
      return;

   wxString skippedMessage;
   std::vector<Node *> nodes = CollectPlotEligibleNodes(skippedMessage);
   if (nodes.empty()) {
      wxString feedback = skippedMessage.IsEmpty() ? wxString("Select one or more sensors with numeric data to plot.") : skippedMessage;
      wxMessageBox(feedback, "Send to Plot", wxOK | wxICON_INFORMATION, this);
      return;
   }

   const bool appended = m_plotManager->AddSensorsToPlot(it->second, nodes);
   if (!appended) {
      wxMessageBox("All selected sensors are already included in that plot.", "Send to Plot", wxOK | wxICON_INFORMATION, this);
      return;
   }

   if (!skippedMessage.IsEmpty())
      wxLogMessage(skippedMessage);
}

void MainFrame::OnItemActivated(wxDataViewEvent &event)
{
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
   PopulatePlotMenu(menu);
   PopupMenu(&menu);
}

void MainFrame::OnExpandAllHere(wxCommandEvent &event)
{
   wxDataViewItem start = m_contextItem.IsOk() ? m_contextItem : wxDataViewItem(NULL);
   ExpandDescendants(m_treeCtrl, start, m_treeModel);
}

void MainFrame::OnCollapseChildrenHere(wxCommandEvent &event)
{
   wxDataViewItem start = m_contextItem.IsOk() ? m_contextItem : wxDataViewItem(NULL);
   CollapseDescendants(m_treeCtrl, start, m_treeModel);
   // Also collapse the starting item itself if it's a valid node
   if (m_contextItem.IsOk()) {
      m_treeCtrl->Collapse(m_contextItem);
      Node *node = static_cast<Node *>(m_contextItem.GetID());
      PruneExpansionSubtree(node, true);
   }
}

void MainFrame::PopulatePlotMenu(wxMenu &menu)
{
   ClearDynamicPlotMenuItems();

   wxMenu *plotMenu = new wxMenu;
   plotMenu->Append(ID_SendToNewPlot, "New Plot...");

   const auto plotNames = m_plotManager->GetPlotNames();
   if (plotNames.empty()) {
      wxMenuItem *noPlots = plotMenu->Append(wxID_ANY, "No existing plots");
      noPlots->Enable(false);
   } else {
      for (const wxString &name : plotNames) {
         const int id = wxWindow::NewControlId();
         plotMenu->Append(id, name);
         m_plotMenuIdToName[id] = name;
         Bind(wxEVT_MENU, &MainFrame::OnSendToExistingPlot, this, id);
      }
   }

   if (menu.GetMenuItemCount() > 0)
      menu.AppendSeparator();
   menu.AppendSubMenu(plotMenu, "Send to Plot");
}

void MainFrame::ClearDynamicPlotMenuItems()
{
   for (const auto &entry : m_plotMenuIdToName) {
      Unbind(wxEVT_MENU, &MainFrame::OnSendToExistingPlot, this, entry.first);
      wxWindow::UnreserveControlId(entry.first);
   }
   m_plotMenuIdToName.clear();
}

void MainFrame::SetupAccelerators()
{
   std::vector<wxAcceleratorEntry> entries;

   auto addShortcut = [&](int flags, int keyCode, int commandId) {
      wxAcceleratorEntry entry;
      entry.Set(flags, keyCode, commandId);
      entries.push_back(entry);
   };

   addShortcut(wxACCEL_CMD, static_cast<int>('F'), ID_FocusFilter);

   if (!entries.empty()) {
      wxAcceleratorTable table(static_cast<int>(entries.size()), entries.data());
      SetAcceleratorTable(table);
   }
}

void MainFrame::OnFocusFilter(wxCommandEvent &WXUNUSED(event))
{
   m_filterCtrl->SetFocus();
   m_filterCtrl->SelectAll();
}

void MainFrame::OnFilterEnter(wxCommandEvent &event)
{
   event.StopPropagation();
   event.Skip(false);
}

std::vector<Node *> MainFrame::CollectPlotEligibleNodes(wxString &messageOut) const
{
   wxDataViewItemArray selections;
   m_treeCtrl->GetSelections(selections);

   if (m_contextItem.IsOk()) {
      bool found = false;
      for (const wxDataViewItem &item : selections) {
         if (item == m_contextItem) {
            found = true;
            break;
         }
      }
      if (!found)
         selections.Add(m_contextItem);
   }

   std::unordered_set<Node *> unique;
   std::vector<Node *> nodes;
   std::vector<wxString> skipped;

   for (const wxDataViewItem &item : selections) {
      Node *node = static_cast<Node *>(item.GetID());
      if (!node)
         continue;

      if (!node->IsLeaf()) {
         skipped.push_back(wxString::FromUTF8(node->GetFullPath().c_str()) + " (not a sensor)");
         continue;
      }

      if (unique.find(node) != unique.end())
         continue;

      const bool hasNumericValue = node->HasValue() && node->GetValue().IsNumeric();
      const bool hasHistory      = node->HasNumericHistory();
      if (!hasNumericValue && !hasHistory) {
         skipped.push_back(wxString::FromUTF8(node->GetFullPath().c_str()) + " (no numeric data)");
         continue;
      }

      unique.insert(node);
      nodes.push_back(node);
   }

   if (!skipped.empty()) {
      wxString summary = "Skipped sensors:\n";
      for (const wxString &label : skipped) {
         summary += "- " + label + "\n";
      }
      messageOut = summary;
   } else {
      messageOut.clear();
   }

   return nodes;
}

void MainFrame::OnRotateLog(wxCommandEvent &WXUNUSED(event))
{
   RotateLogFile("Log rotated manually.");
}

void MainFrame::OnClearTree(wxCommandEvent &WXUNUSED(event))
{
   m_treeCtrl->Freeze();
   m_treeCtrl->UnselectAll();
   m_treeModel->Clear();
   m_expandedNodes.clear();
   m_treeCtrl->Thaw();
}

void MainFrame::OnSavePlotConfig(wxCommandEvent &WXUNUSED(event))
{
   const auto configs = m_plotManager->GetPlotConfigurations();
   if (configs.empty()) {
      wxMessageBox("There are no plots to save.", "Save Plot Configuration", wxOK | wxICON_INFORMATION, this);
      return;
   }

   wxFileDialog dialog(this, "Save Plot Configuration", wxEmptyString, "plot-config.ini",
       "Config files (*.ini)|*.ini|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
   if (dialog.ShowModal() != wxID_OK)
      return;

   wxFileName fileName(dialog.GetPath());
   if (fileName.GetExt().IsEmpty())
      fileName.SetExt("ini");

   wxFileConfig fileConfig(wxEmptyString, wxEmptyString, fileName.GetFullPath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE);
   fileConfig.DeleteAll();
   fileConfig.SetPath("/");

   std::unordered_set<std::string> usedSections;
   for (size_t idx = 0; idx < configs.size(); ++idx) {
      const auto &entry = configs[idx];
      wxString section  = entry.name;
      section.Trim(true);
      section.Trim(false);
      if (section.IsEmpty())
         section = wxString::Format("Plot_%zu", idx + 1);

      const wxUniChar forbidden[] = {'/', '\\', '[', ']', ':', ';', '='};
      for (wxUniChar ch : forbidden) {
         wxString target(ch);
         section.Replace(target, "_", true);
      }

      if (section.IsEmpty())
         section = wxString::Format("Plot_%zu", idx + 1);

      wxString candidate = section;
      int suffix         = 1;
      std::string key    = ToUtf8(candidate);
      if (key.empty())
         key = "section_" + std::to_string(idx);
      while (usedSections.count(key) > 0) {
         candidate = section + wxString::Format("_%d", suffix++);
         key       = ToUtf8(candidate);
         if (key.empty())
            key = "section_" + std::to_string(idx) + "_" + std::to_string(suffix);
      }
      usedSections.insert(key);

      fileConfig.SetPath("/" + candidate);
      fileConfig.Write("Title", entry.name);
      for (size_t sensorIdx = 0; sensorIdx < entry.sensorPaths.size(); ++sensorIdx) {
         const std::string &path = entry.sensorPaths[sensorIdx];
         const wxString value    = wxString::FromUTF8(path.c_str());
         fileConfig.Write(wxString::Format("Sensor%zu", sensorIdx + 1), value);
      }
      fileConfig.SetPath("..");
   }

   fileConfig.Flush();

   wxLogMessage("Plot configuration saved to %s.", fileName.GetFullPath());
}

void MainFrame::OnLoadPlotConfig(wxCommandEvent &WXUNUSED(event))
{
   if (!m_plotManager)
      return;

   wxFileDialog dialog(this, "Load Plot Configuration", wxEmptyString, wxEmptyString,
       "Config files (*.ini)|*.ini|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
   if (dialog.ShowModal() != wxID_OK)
      return;

   wxFileConfig fileConfig(wxEmptyString, wxEmptyString, dialog.GetPath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE);
   fileConfig.SetPath("/");

   wxString group;
   long groupCookie = 0;
   std::vector<PlotManager::PlotConfiguration> configs;

   bool hasGroup = fileConfig.GetFirstGroup(group, groupCookie);
   while (hasGroup) {
      fileConfig.SetPath("/" + group);

      wxString title;
      if (!fileConfig.Read("Title", &title))
         title = group;
      title.Trim(true);
      title.Trim(false);

      std::vector<std::pair<int, std::string>> sensors;
      wxString entry;
      long entryCookie = 0;
      bool hasEntry    = fileConfig.GetFirstEntry(entry, entryCookie);
      while (hasEntry) {
         if (entry.CmpNoCase("Title") != 0) {
            wxString value;
            if (fileConfig.Read(entry, &value)) {
               long order = static_cast<long>(sensors.size());
               if (entry.StartsWith("Sensor")) {
                  wxString suffix = entry.Mid(6);
                  if (!suffix.ToLong(&order))
                     order = static_cast<long>(sensors.size());
               }
               const wxScopedCharBuffer buffer = value.ToUTF8();
               if (buffer && buffer.length() > 0) {
                  sensors.emplace_back(static_cast<int>(order), std::string(buffer.data(), buffer.length()));
               }
            }
         }
         hasEntry = fileConfig.GetNextEntry(entry, entryCookie);
      }

      std::sort(sensors.begin(), sensors.end(),
          [](const auto &lhs, const auto &rhs) {
             return lhs.first < rhs.first;
          });

      PlotManager::PlotConfiguration cfg;
      cfg.name = title;
      for (auto &sensor : sensors) {
         if (!sensor.second.empty())
            cfg.sensorPaths.push_back(std::move(sensor.second));
      }
      configs.push_back(std::move(cfg));

      fileConfig.SetPath("..");
      hasGroup = fileConfig.GetNextGroup(group, groupCookie);
   }

   if (configs.empty()) {
      wxMessageBox("No plot sections found in the selected configuration.", "Load Plot Configuration", wxOK | wxICON_INFORMATION, this);
      return;
   }

   if (!m_plotManager->GetPlotNames().empty()) {
      const int response = wxMessageBox("Loading a configuration will close all existing plots. Continue?", "Load Plot Configuration",
          wxYES_NO | wxICON_QUESTION, this);
      if (response != wxYES)
         return;
      m_plotManager->CloseAllPlots();
   }

   std::vector<wxString> warnings;
   const size_t created = m_plotManager->RestorePlotConfigurations(configs, warnings);

   if (created == 0) {
      wxMessageBox("No plots were created from the configuration.", "Load Plot Configuration", wxOK | wxICON_WARNING, this);
   } else {
      wxLogMessage("Loaded %zu plot(s) from configuration.", created);
   }

   if (!warnings.empty()) {
      wxString message = "Some sensors could not be restored:\n";
      for (const wxString &line : warnings)
         message += "- " + line + "\n";
      wxMessageBox(message, "Load Plot Configuration", wxOK | wxICON_INFORMATION, this);
   }
}

void MainFrame::OnConnectionStatus(wxThreadEvent &event)
{
   switch (event.GetId()) {
      case ID_ConnectYes: {
         const bool wasConnected = m_isNetworkConnected;
         m_isNetworkConnected    = true;
         UpdateNetworkIndicator(*wxGREEN, "Network connected");
         if (!wasConnected) {
            RotateLogFile("Network connected; new log file started.");
         }
         break;
      }
      case ID_ConnectNo: {
         m_isNetworkConnected = false;
         UpdateNetworkIndicator(*wxYELLOW, "Network idle");
         CloseLogFile("Connection lost; log file closed.");
         break;
      }
      default:
         break;
   }
}

void MainFrame::OnNewMessage(wxThreadEvent &WXUNUSED(event))
{
   ++m_messagesReceived;
   SetStatusText(wxString::Format("Messages received: %zu", (unsigned long long)m_messagesReceived), STATUS_FIELD_MESSAGE_COUNT);
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
   // Uncheck the menu item if present on this frame
   if (GetMenuBar()) {
      wxMenuItem *mi = GetMenuBar()->FindItem(ID_ToggleDataGen);
      if (mi)
         mi->Check(false);
   }
}

void MainFrame::RotateLogFile(const wxString &reason)
{
   m_dataRecorder.reset();

   m_currentLogFile = SensorDataJsonWriter::GenerateTimestampedFilename();
   m_dataRecorder   = std::make_unique<SensorDataJsonWriter>(m_currentLogFile);

   const wxString logFile = wxString::FromUTF8(m_currentLogFile.c_str());
   wxString logStatus;

   if (m_dataRecorder->IsOpen()) {
      logStatus = "Current log: " + logFile;
   } else {
      logStatus = "Current log: " + logFile + " (open failed)";
      wxLogError("Unable to open log file '" + logFile + "'.");
   }

   SetStatusText(logStatus, STATUS_FIELD_LOG_INFO);
}

void MainFrame::CloseLogFile(const wxString &reason)
{
   m_dataRecorder.reset();
   m_currentLogFile.clear();

   SetStatusText("Current log: (no active log)", STATUS_FIELD_LOG_INFO);
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

   m_plotManager->CloseAllPlots();
   m_plotManager.reset();

   // Ensure the data view control disassociates the model before it is destroyed.
   m_treeCtrl->AssociateModel(nullptr);
   delete m_treeModel;
   m_treeModel = nullptr;

   m_dataRecorder.reset();

   // Proceed with default close handling
   event.Skip();
}