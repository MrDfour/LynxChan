{
  "targets": [
    {
      "target_name": "native",
      "sources": [
        "native/main.cpp",
        "native/captcha.cpp",
        "native/imageBounds.cpp",
        "native/videoHandler.cpp",
        "native/imageThumb.cpp"
      ],
      "include_dirs": [
        "<!@(pkg-config --cflags-only-I Magick++ | sed s/-I//g)",
        "<!@(pkg-config --cflags-only-I libavformat | sed s/-I//g)",
        "<!@(pkg-config --cflags-only-I libavcodec | sed s/-I//g)",
        "<!@(pkg-config --cflags-only-I libavutil | sed s/-I//g)",
        "<!@(pkg-config --cflags-only-I libswscale | sed s/-I//g)",
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "link_settings": {
        "libraries": [
          "<!@(pkg-config --libs-only-l Magick++)",
          "-lavformat -lavcodec -lavutil -lswscale"
        ]
      },
      "cflags_cc!": [
        "-fno-exceptions"
      ],
      "defines": [
        "MAGICKCORE_HDRI_ENABLE=0",
        "MAGICKCORE_QUANTUM_DEPTH=16"
      ]
    }
  ]
}
