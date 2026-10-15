# Nayuki's QR Code generator library (C), v1.8.0, MIT. See LICENSE.txt.
add_library(qrcodegen STATIC qrcodegen.c qrcodegen.h)
dolphin_disable_warnings(qrcodegen)
target_include_directories(qrcodegen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
