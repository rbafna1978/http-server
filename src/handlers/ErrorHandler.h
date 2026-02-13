#pragma once

#include <string>

#include "http/HttpResponse.h"

namespace handlers {

http::HttpResponse create400(const std::string& error);
http::HttpResponse create404();
http::HttpResponse create405();
http::HttpResponse create429();
http::HttpResponse create500(const std::string& error);

}  // namespace handlers
