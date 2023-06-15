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

    auto json_helper(juce::var& object, juce::String key, juce::var value ){
        if( object.isObject() ) {
            object.getDynamicObject()->setProperty(key, value);
        }
    }

    bool process_bytes(const string& audioWavBytes, const string& outputFileName) const {
        // Create an HTTP client object
        try {

            // ------------------------
            // PREPARE THE POST REQUEST
            // ------------------------

            URL url(m_url + m_api_name);
            // URL url("https://abidlabs-whisper.hf.space/run/predict");
            // TODO : check the options in juce_URL.h (e.g. withFileToUpload, withPostData etc)
            StringPairArray responseHeaders;
            int statusCode = 0;


            int file_size = audioWavBytes.size();
            String audioWavBase64Juce = String(audioWavBytes.c_str());
            
            // A discussion  about building and parsing JSON in JUCE:
            // One can figure out a much more elegant way than this. 
            String jsonString = "{" \
                "\"data\": [{" \
                    "\"orig_name\": \"audio.wav\"," \
                    "\"name\": \"input_blabla.wav\"," \
                    "\"data\": \"" + audioWavBase64Juce + "\"," \
                    "\"is_file\": false," \
                    "\"size\": " + String(file_size) + \
                "}]," \
                "\"fn_index\": 4," \
                "\"session_hash\": 111111111111111" \
            "}";

            // Write the JSON object (request body) to a file for debugging purposes
            // juce::File filePostBody("C:\\Users\\xribene\\AppData\\Local\\Temp\\postBody.txt");
            juce::File filePostBody = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("postBody.txt");
            filePostBody.replaceWithText(jsonString, false);

            // Directly feed the POST data into the URL object
            if(!jsonString.isEmpty()) url = url.withPOSTData(jsonString);

            // Create the HTTP request and set the request method, headers, and body
            // ParameterHandling::inPostData
            std::unique_ptr<InputStream> requestStream(url.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                                    // .withConnectionTimeoutMs (1000)
                                    // .withNumRedirectsToFollow (0)
                                    .withExtraHeaders("Content-Type: application/json")
                                    // We probably don't need that since we use withPOSTData above
                                    .withHttpRequestCmd ("POST") 
                                    // can we use that to create a progress bar?
                                    // .withProgressCallback 
                                    .withStatusCode (&statusCode)
                                    // .withPOSTData (postData)
                                    .withResponseHeaders (&responseHeaders)
                                    // other options in juce_URL.h
                                    )
            );

            String result = requestStream->readEntireStreamAsString();
            String head = responseHeaders.getDescription().toStdString();
            if (statusCode != 200)
            {
                DBG("Error: " + String(statusCode));
                DBG("Error: " + result);
                return false;
            }

            

            // For vampnet, Gradio doesn't return the audio file in the response body,
            // but it gives as the wav file location in the remote server
            // We have to make a separate GET request to get it

            // ------------------------
            // PREPARE THE GET REQUEST
            // ------------------------

            // Parse the JSON response and find the location of the generated wav file
            var varResult = JSON::parse(result);
            String generated_file_name;
            var data = varResult.getProperty("data", var());
            auto* data_array = data.getArray();
            var* element = data_array->begin();
            var name = element->getProperty("name", var());
            generated_file_name = name.toString();
            // Check if generated_file_name is empty
            if (generated_file_name.isEmpty()){
                DBG("Error: Failed to parse JSON response.");
                return false;
            }
            URL urlGET(m_url + "/file=" + generated_file_name.toStdString());
            StringPairArray responseHeadersGET;
            int statusCodeGET = 0;

            // Create the HTTP GET request and set the request method, headers, and body
            std::unique_ptr<InputStream> requestStreamGET(urlGET.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                                    // .withConnectionTimeoutMs (1000)
                                    // .withNumRedirectsToFollow (0)
                                    // .withExtraHeaders("Content-Type: application/json")
                                    // We probably don't need that since we use withPOSTData above
                                    .withHttpRequestCmd ("GET") 
                                    // can we use that to create a progress bar?
                                    // .withProgressCallback 
                                    .withStatusCode (&statusCodeGET)
                                    // .withPOSTData (postData)
                                    .withResponseHeaders (&responseHeadersGET)
                                    // other options in juce_URL.h
                                    )
            );

            if (statusCodeGET != 200)
            {
                DBG("Error: " + String(statusCodeGET));
                // DBG("Error: " + resultGET);
                return false;
            }

            String headGET = responseHeadersGET.getDescription().toStdString();
            // String resultGET = responseStreamGET->readEntireStreamAsString();
            juce::MemoryBlock wavData;

            requestStreamGET->readIntoMemoryBlock(wavData, 300000);
            String outputBase64 = wavData.toBase64Encoding();
            juce::File outputFile = juce::File(outputFileName);
            juce::FileOutputStream outputStream (outputFile);
            outputStream.write(wavData.getData(), wavData.getSize());
            outputStream.flush();

            return true;

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
        // TODO : check juce::TemporaryFile
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
