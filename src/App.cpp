#include "MainFrame.h"

#include <wx/wx.h>

class App : public wxApp
{
 public:
   virtual bool OnInit() override;
};

wxIMPLEMENT_APP(App);

bool App::OnInit()
{
   if (!wxApp::OnInit())
      return false;

   MainFrame *frame = new MainFrame();
   frame->Show(true);

   return true;
}