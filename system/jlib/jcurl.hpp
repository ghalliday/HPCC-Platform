#ifndef JCURL_HPP
#define JCURL_HPP

#include "jiface.hpp"
#include "jstring.hpp"

#include <map>
#include <string>

// Minimal IHttpClient abstracting libcurl operations
interface IJlibHttpClient : extends IInterface
{
    virtual void setBasicAuth(const char* username, const char* password) = 0;
    virtual void setVerifyServer(bool verify) = 0;
    virtual void setClientCert(const char* certPath, const char* keyPath) = 0;
    
    // Timeouts
    virtual void setTimeouts(long connectTimeoutMs, long readTimeoutMs, long writeTimeoutMs) = 0;

    // Header management
    virtual void setHeaders(const std::map<std::string, std::string>& headers) = 0;
    virtual void addHeader(const char* key, const char* value) = 0;

    // HTTP Verbs (returns the HTTP status code, updates responseBody)
    virtual int get(const char* path, StringBuffer& responseBody) = 0;
    virtual int post(const char* path, const char* body, const char* contentType, StringBuffer& responseBody) = 0;
    virtual int put(const char* path, const char* body, const char* contentType, StringBuffer& responseBody) = 0;

    virtual const char* getErrorMessage() const = 0;
    virtual int getErrorCode() const = 0;
};

// Factory method
extern jlib_decl IJlibHttpClient* createJlibHttpClient(const char* baseUrl);

#endif // JCURL_HPP
