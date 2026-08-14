#include <iostream>
#include <string>

#include <tag.h>
#include <fileref.h>

#include <mpegfile.h>
#include <flacfile.h>
#include <vorbisfile.h>
#include <wavfile.h>

void printAudioMetadata(const std::string& filePath) {
    std::cout << "========================================" << std::endl;
    std::cout << "Reading metadata for: " << filePath << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        TagLib::FileRef file(filePath.c_str());

        if (!file.isNull() && file.tag()) {
            TagLib::Tag* tag = file.tag();

            // Basic metadata
            std::cout << "Title:    " << tag->title().to8Bit(true) << std::endl;
            std::cout << "Artist:   " << tag->artist().to8Bit(true) << std::endl;
            std::cout << "Album:    " << tag->album().to8Bit(true) << std::endl;
            std::cout << "Comment:  " << tag->comment().to8Bit(true) << std::endl;
            std::cout << "Genre:    " << tag->genre().to8Bit(true) << std::endl;
            std::cout << "Year:     " << tag->year() << std::endl;
            std::cout << "Track:    " << tag->track() << std::endl;

            // Audio properties
            if (file.audioProperties()) {
                TagLib::AudioProperties* props = file.audioProperties();
                std::cout << "----------------------------------------" << std::endl;
                std::cout << "Audio Properties:" << std::endl;
                std::cout << "Duration: " << props->lengthInSeconds() << " seconds ("
                          << static_cast<short>(props->lengthInSeconds()) / 60 << ":"
                          << props->lengthInSeconds() - static_cast<short>(props->lengthInSeconds() / 60) * 60 << ")"
                          << std::endl;
                std::cout << "Bitrate:  " << props->bitrate() << " kbps" << std::endl;
                std::cout << "Sample Rate: " << props->sampleRate() << " Hz" << std::endl;
                std::cout << "Channels: " << props->channels() << std::endl;
            }
        } else {
            std::cerr << "Error: Could not read metadata (file is null or no tag)" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception occurred: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown error reading file" << std::endl;
    }

    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    const std::string testFiles[] = {
        "Your audio media's file path",
    };

    if (argc > 1) {
        printAudioMetadata(argv[1]);
        return 0;
    }

    for (const auto& file : testFiles) {
        printAudioMetadata(file);
    }

    return 0;
}
