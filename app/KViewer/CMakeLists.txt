set(KViewer_SRCS
    krviewer.cpp
    panelviewer.cpp
    diskusageviewer.cpp
    imagebrowser.cpp
    lister.cpp
    viewertabbar.cpp)

add_library(KViewer STATIC ${KViewer_SRCS})

target_link_libraries(KViewer
    Qt6::PrintSupport
    Qt6::Core5Compat
    KF6::Archive
    KF6::Codecs
    KF6::ConfigCore
    KF6::ConfigWidgets
    KF6::CoreAddons
    KF6::IconThemes
    KF6::KIOWidgets
    KF6::KIOCore
    KF6::KIOFileWidgets
    KF6::TextWidgets
    KF6::Service
    KF6::WindowSystem
    KF6::Parts
    KF6::WidgetsAddons
    KF6::XmlGui
)

# Standalone unit test for ImageBrowser
if(BUILD_TESTING)
    add_executable(KViewerTestImageBrowser
        test_imagebrowser.cpp
        imagebrowser.cpp
    )
    target_link_libraries(KViewerTestImageBrowser
        PRIVATE
        Qt6::Widgets
        KF6::Parts
        KF6::KIOCore
        KF6::ConfigCore
        KF6::CoreAddons
    )
endif()
