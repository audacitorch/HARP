#pragma once 

// #include <fstream>
// #include <cpprest/http_client.h>
// #include <cpprest/filestream.h>
// #include <cpprest/base64.h>

// using namespace web::http;
// using namespace web::http::client;

#include "Model.h"


class WebModel : public Model
{
public:
    virtual bool load(const map<string, any> &params) override {
        // get the name of the huggingface repo we're going to use
        if (!params.contains("url")) {
            return false;
        }
        if(!params.contains("api_name")){
            return false;
        }
        m_url = any_cast<string>(params.at("url"));
        m_api_name = any_cast<string>(params.at("api_name"));

        m_loaded = true;
        return true;
    }

    virtual bool ready() const override {
        return m_loaded;
    }

    // read audio file to a vector of bytes
    bool read_audio_file_to_base64(const string& filePath, string& output) const {

        File file(filePath);
        if (!file.exists())
        {
            DBG("Error: Fille doesn't exist.");
            return false;
        }

        FileInputStream inputStream(file);
        if (!inputStream.openedOk())
        {
            // Handle file opening error
            DBG("Error: Failed to open file.");
            return false;
        }

        // get the file size
        // use juce::int64 for cross-platform compatibility
        const juce::int64 fileSize = inputStream.getTotalLength();

        MemoryBlock buffer(fileSize);
        if (inputStream.read(buffer.getData(), fileSize) != fileSize)
        {
            DBG("Failed to read file: " + filePath);
            std::cerr << "Failed to read file: " << filePath << std::endl;
            return false;
        }

        // Buffer to Base64
        String base64String = juce::Base64::toBase64(buffer.getData(), buffer.getSize());
        // Juce::String to std::string
        output = base64String.toStdString();

        return true;
    }


    bool process_bytes(const string& audioWavBytes, const string& outputFileName) const {
        // Create an HTTP client object
        try {

            
            URL url(m_url + m_api_name);
            // TODO : check the options in juce_URL.h (e.g. withFileToUpload, withPostData etc)
            StringPairArray responseHeaders;
            int statusCode = 0;

            // an example
            DynamicObject* obj = new DynamicObject();
            obj->setProperty("grant_type","client_credentials");
            obj->setProperty("client_id","id");
            obj->setProperty("client_secret","secret");
            var json (obj);
            String s = JSON::toString(json);

            // Directly feed the POST data into the URL object
            if(!s.isEmpty()) url = url.withPOSTData(s);

            // Create a JSON object with the request data
            // var requestData = var::fromJSON("{" \
            //     "\"data\": [{" \
            //         "\"fn_index\": 4," \
            //         "\"data\": {" \
            //             "\"orig_name\": \"audio.wav\"," \
            //             "\"data\": \"" + audioWavBytes + "\"," \
            //             "\"is_file\": false," \
            //             "\"size\": " + String(file_size) + \
            //         "}" \
            //     "}]" \
            // "}");

            // Create the HTTP request and set the request method, headers, and body
            // ParameterHandling::inPostData
            std::unique_ptr<InputStream> requestStream(url.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                                                                                        // .withConnectionTimeoutMs (1000)
                                                                                        // .withNumRedirectsToFollow (0)
                                                                                        .withExtraHeaders("Content-Type: application/json")
                                                                                        .withHttpRequestCmd ("POST")
                                                                                        // .withProgressCallback // can we use that to create a progress bar?
                                                                                        .withStatusCode (&statusCode)
                                                                                        // .withPOSTData (postData)
                                                                                        .withResponseHeaders (&responseHeaders)
                                                                                        // other options in juce_URL.h
                                                                            )
                                                    );
            // MemoryBlock requestBody;
            // JSON::writeToStream(requestBody, requestData);
            // requestStream->write(requestBody.getData(), requestBody.getSize());

            String result = requestStream->readEntireStreamAsString();
            String head = responseHeaders.getDescription().toStdString();
            if (statusCode != 200)
            {
                DBG("Error: " + String(statusCode));
                DBG("Error: " + result);
                return false;
            }

            // High level JUCE code to write the result to a file
            File outputFile(outputFileName);
            outputFile.replaceWithText(result, false);
            // // Lower level version
            // File outputFile(outputFileName);
            // FileOutputStream outputStream(outputFile);

            // if (outputStream.openedOk())
            // {
            //     outputStream << result;
            //     outputStream.flush();
            //     outputStream.close();
            // }
            // else
            // {
            //     DBG("Error: Failed to open file for writing.");
            //     return false;
            // }

            // // Send the HTTP request and read the response
            // std::unique_ptr<InputStream> responseStream(url.createInputStream(false));
            // MemoryBlock responseBody;
            // responseStream->readIntoMemoryBlock(responseBody, -1);

            // // Process the response
            // String responseString(responseBody.toString());
            // // ... do something with the response ...





            // http_client_config config;
            // config.set_validate_certificates(false);

            // http_client client(U(m_url+m_api_name), config);
            // // DBG("Sending request to " + m_url + m_api_name);

            // int file_size = audioWavBytes.size();

            // // Create a JSON object with the request data
            // web::json::value requestData;
            // requestData[U("data")] = web::json::value::array({ web::json::value::object({
            //     { U("fn_index"), web::json::value::number(4) },
            //     { U("data"), web::json::value::object({
            //             {U("orig_name"), web::json::value::string("audio.wav")},
            //             {U("data"), web::json::value::string(audioWavBytes)},
            //             {U("is_file"), web::json::value::boolean(false)},
            //             {U("size"), web::json::value::number(file_size)},
            //         }),
            //     },
            //     }) 
            // });

            // // Create the HTTP request and set the request URI
            // http_request request(methods::POST);
            // // request.set_request_uri(m_api_name);
            // request.headers().set_content_type(U("application/json"));
            // request.set_body(requestData);

            // // Send the HTTP request and wait for the response
            // pplx::task<http_response> response = client.request(request);
            // response.wait();
            
            // if (response.get().status_code() != 200) {
            //     DBG("Error: " + std::to_string(response.get().status_code()));
            //     DBG("Error: " + response.get().extract_utf8string().get());
            //     return false;
            // }

            // // Save the response data to a file
            // std::ofstream outputFile(outputFileName);
            // outputFile << response.get().extract_utf8string().get();
            // outputFile.close();
            // return true;
        } catch (const std::exception& e) {
            DBG("Error: " + string(e.what()));
            return false;
        }
    }


private:
    string m_url {};
    string m_api_name {};
    bool m_loaded = false;

};

class WebWave2Wave : public WebModel, public Wave2Wave
{
public:

    virtual void process(juce::AudioBuffer<float> *bufferToProcess, int sampleRate) const override {
        // save the buffer to file
        juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("input.wav");
        if (!save_buffer_to_file(*bufferToProcess, tempFile, sampleRate)) {
            DBG("Failed to save buffer to file.");
            return;
        }

        // read the file to a vector of bytes
        string audioWavBytes;
        if (!read_audio_file_to_base64(tempFile.getFullPathName().toStdString(), audioWavBytes)) {
            DBG("Failed to read audio file to base64.");
            return;
        }
        DBG("Read audio file to base64. Size: " + std::to_string(audioWavBytes.size()) + " bytes.");

        // process the bytes
        juce::File tempOutputFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("output.wav");
        if (!process_bytes(audioWavBytes, tempOutputFile.getFullPathName().toStdString())){
            DBG("Failed to process bytes.");
            return;
        }


        // read the output file to a buffer
        // TODO: we're gonna have to resample here? 
        int newSampleRate;
        load_buffer_from_file(tempOutputFile, *bufferToProcess, newSampleRate);
        jassert (newSampleRate == sampleRate);

        // delete the temp input file
        tempFile.deleteFile();
        tempOutputFile.deleteFile();


        return;
    }

};
