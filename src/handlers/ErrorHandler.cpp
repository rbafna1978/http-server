#include "handlers/ErrorHandler.h"

namespace handlers {

http::HttpResponse create400(const std::string& error) {
    http::HttpResponse resp;
    resp.setStatus(400, "Bad Request");
    resp.setContentType("text/html");
    resp.setBody("<html><body><h1>400 Bad Request</h1><p>" + error + "</p></body></html>");
    return resp;
}

http::HttpResponse create404() {
    http::HttpResponse resp;
    resp.setStatus(404, "Not Found");
    resp.setContentType("text/html");
    resp.setBody("<html><body><h1>404 Not Found</h1></body></html>");
    return resp;
}

http::HttpResponse create405() {
    http::HttpResponse resp;
    resp.setStatus(405, "Method Not Allowed");
    resp.setContentType("text/html");
    resp.setBody("<html><body><h1>405 Method Not Allowed</h1></body></html>");
    return resp;
}

http::HttpResponse create429() {
    http::HttpResponse resp;
    resp.setStatus(429, "Too Many Requests");
    resp.setContentType("text/html");
    resp.setBody("<html><body><h1>429 Too Many Requests</h1></body></html>");
    return resp;
}

http::HttpResponse create500(const std::string& error) {
    http::HttpResponse resp;
    resp.setStatus(500, "Internal Server Error");
    resp.setContentType("text/html");
    resp.setBody("<html><body><h1>500 Internal Server Error</h1><p>" + error + "</p></body></html>");
    return resp;
}

}  // namespace handlers
