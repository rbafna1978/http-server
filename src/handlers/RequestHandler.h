#pragma once

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

class RequestHandler {
public:
    virtual http::HttpResponse handle(const http::HttpRequest& request) = 0;
    virtual ~RequestHandler() = default;
};
