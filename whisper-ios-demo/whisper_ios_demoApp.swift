//
//  whisper_ios_demoApp.swift
//  whisper-ios-demo
//
//  Created by Ben Nortier on 2022/10/12.
//

import AudioKit
import SwiftUI

@main
struct whisper_ios_demoApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
                .onOpenURL(perform: handleURL)
        }
    }

    func handleURL(_ inputURL: URL) {
        var options = FormatConverter.Options()
        options.format = .wav
        options.sampleRate = 16000
        options.bitDepth = 16

        let outputURL = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent("output-\(UUID()).wav")
        print(outputURL)

        func cb(_ progress: UnsafePointer<Int8>?) -> Int32 {
            let str = String(cString: progress!)
            print(str)
            return 0
        }

        let converter = FormatConverter(inputURL: inputURL, outputURL: outputURL, options: options)
        converter.start { error in
            if error == nil {
                DispatchQueue.global(qos: .userInitiated).async {
                    let modelURL = Bundle.main.url(forResource: "ggml-tiny", withExtension: "bin", subdirectory: "")
                    read_wav(modelURL!.absoluteURL.path, outputURL.absoluteURL.path, cb)

                    DispatchQueue.main.async {
                        print("This is run on the main queue, after the previous code in outer block")
                    }
                }
            }
        }
    }
}
