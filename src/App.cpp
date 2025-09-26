#include "App.hpp"

#include "MainFrame.h"

#include <wx/socket.h>
#include <wx/wx.h>

class App : public wxApp
{
 public:
   virtual bool OnInit() override;
};

wxIMPLEMENT_APP(App);

bool App::OnInit()
{
   wxSocketBase::Initialize();

   if (!wxApp::OnInit())
      return false;

   MainFrame *frame = new MainFrame();
   frame->Show(true);

   return true;
}