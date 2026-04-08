#include "platform.h"
#include "jcurl.hpp"
#include "jlog.hpp"
#include "jexcept.hpp"

#include <curl/curl.h>
#include <vector>

class CJlibHttpClient : public CInterfaceOf<IJlibHttpClient>
{
private:
    CURL* curl = nullptr;
    std::string baseUrl;
    std::map<std::string, std::string> currentHeaders;
    std::string errorMsg;
    int lastErrorCode = 0;
    bool verifyServer = true;
    long connectTimeoutMs = 0;
    long readTimeoutMs = 0;
    long writeTimeoutMs = 0;

    struct curl_slist* buildCurlHeaders(const char* contentType)
    {
        struct curl_slist* chunk = nullptr;
        for (const auto& pair : currentHeaders)
        {
            std::string headerLine = pair.first + ": " + pair.second;
            chunk = curl_slist_append(chunk, headerLine.c_str());
        }
        if (contentType)
        {
            std::string ctLine = std::string("Content-Type: ") + contentType;
            chunk = curl_slist_append(chunk, ctLine.c_str());
        }
        return chunk;
    }

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp)
    {
        size_t realsize = size * nmemb;
        StringBuffer* buf = static_cast<StringBuffer*>(userp);
        buf->append(realsize, (const char*)contents);
        return realsize;
    }

    int performRequest(const char* path, struct curl_slist* headers, StringBuffer& responseBody)
    {
        errorMsg.clear();
        lastErrorCode = 0;
        
        std::string fullUrl = baseUrl + path;
        curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, verifyServer ? 1L : 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, verifyServer ? 2L : 0L);
        
        if (connectTimeoutMs > 0)
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connectTimeoutMs);
        if (readTimeoutMs > 0 || writeTimeoutMs > 0)
        {
            // libcurl doesn't have separate read/write timeouts easily decoupled, use MAX for CURLOPT_TIMEOUT_MS
            long overallTimeout = std::max(readTimeoutMs, writeTimeoutMs);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, overallTimeout);
        }

        char errbuf[CURL_ERROR_SIZE];
        errbuf[0] = 0;
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            lastErrorCode = res;
            size_t len = strlen(errbuf);
            if (len)
                errorMsg = errbuf;
            else
                errorMsg = curl_easy_strerror(res);
            return -1;
        }

        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        return static_cast<int>(httpCode);
    }

public:
    CJlibHttpClient(const char* _baseUrl) : baseUrl(_baseUrl)
    {
        curl = curl_easy_init();
    }

    ~CJlibHttpClient()
    {
        if (curl)
            curl_easy_cleanup(curl);
    }

    virtual void setBasicAuth(const char* username, const char* password) override
    {
        std::string auth = std::string(username) + ":" + password;
        curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
    }

    virtual void setVerifyServer(bool verify) override
    {
        verifyServer = verify;
    }

    virtual void setClientCert(const char* certPath, const char* keyPath) override
    {
        curl_easy_setopt(curl, CURLOPT_SSLCERT, certPath);
        curl_easy_setopt(curl, CURLOPT_SSLKEY, keyPath);
    }

    virtual void setTimeouts(long _connectTimeoutMs, long _readTimeoutMs, long _writeTimeoutMs) override
    {
        connectTimeoutMs = _connectTimeoutMs;
        readTimeoutMs = _readTimeoutMs;
        writeTimeoutMs = _writeTimeoutMs;
    }

    virtual void setHeaders(const std::map<std::string, std::string>& headers) override
    {
        currentHeaders = headers;
    }

    virtual void addHeader(const char* key, const char* value) override
    {
        currentHeaders[key] = value;
    }

    virtual int get(const char* path, StringBuffer& responseBody) override
    {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        struct curl_slist* chunk = buildCurlHeaders(nullptr);
        int res = performRequest(path, chunk, responseBody);
        if (chunk) curl_slist_free_all(chunk);
        return res;
    }

    virtual int post(const char* path, const char* body, const char* contentType, StringBuffer& responseBody) override
    {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        
        struct curl_slist* chunk = buildCurlHeaders(contentType);
        int res = performRequest(path, chunk, responseBody);
        if (chunk) curl_slist_free_all(chunk);
        return res;
    }

    virtual int put(const char* path, const char* body, const char* contentType, StringBuffer& responseBody) override
    {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        
        struct curl_slist* chunk = buildCurlHeaders(contentType);
        int res = performRequest(path, chunk, responseBody);
        if (chunk) curl_slist_free_all(chunk);
        return res;
    }

    virtual const char* getErrorMessage() const override
    {
        return errorMsg.empty() ? nullptr : errorMsg.c_str();
    }
    
    virtual int getErrorCode() const override
    {
        return lastErrorCode;
    }
};

IJlibHttpClient* createJlibHttpClient(const char* baseUrl)
{
    return new CJlibHttpClient(baseUrl);
}
