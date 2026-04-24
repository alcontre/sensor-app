#pragma once

#include <wx/string.h>

#include <sstream>
#include <string>
#include <vector>

namespace PathUtils {

inline std::string ToUtf8(const wxString &value)
{
   const wxScopedCharBuffer buffer = value.ToUTF8();
   if (!buffer)
      return "";

   return std::string(buffer.data(), buffer.length());
}

inline std::vector<std::string> SplitPath(const std::string &path)
{
   std::vector<std::string> segments;
   size_t start = 0;
   while (start < path.size()) {
      size_t end = path.find('/', start);
      if (end == std::string::npos)
         end = path.size();

      if (end > start)
         segments.emplace_back(path.substr(start, end - start));

      start = end + 1;
   }

   return segments;
}

inline std::string JoinPath(const std::vector<std::string> &path)
{
   std::ostringstream oss;
   for (size_t i = 0; i < path.size(); ++i) {
      if (i > 0)
         oss << '/';
      oss << path[i];
   }

   return oss.str();
}

} // namespace PathUtils