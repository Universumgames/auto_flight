
if (NOT NATIVE_BUILD)
    FILE(GLOB cnn_srcs "*.cpp")

    idf_component_register(
            SRCS ${cnn_srcs} "${CMAKE_CURRENT_BINARY_DIR}/model_data.cc"
            INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
            REQUIRES freertos esp_timer flight_com camera
    )

    # Generates model_data.h / model_data.cc (g_model, kLabels) from the trained int8 model in
    # embedded_vision_training (see model_helper/cExport.py; `make pipeline` there produces the
    # model). Written to this component's binary dir, which is a public include dir above.
    set(CNN_TRAINING_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../embedded_vision_training")
    set(CNN_MODEL_TFLITE "${CNN_TRAINING_DIR}/models/fire_smoke_cnn_int8.tflite")
    set(CNN_MODEL_EXPORT "${CNN_TRAINING_DIR}/model_helper/cExport.py")
    set(CNN_MODEL_OUT "${CMAKE_CURRENT_BINARY_DIR}/model_data.h" "${CMAKE_CURRENT_BINARY_DIR}/model_data.cc")

    foreach (input IN ITEMS "${CNN_MODEL_TFLITE}")
        if (NOT EXISTS "${input}")
            message(FATAL_ERROR "cnn: ${input} not found, train and convert the model first "
                    "(make pipeline in embedded_vision_training)")
        endif ()
    endforeach ()

    # The generated ModelOpResolver registers the model's ops via the Add* methods of this
    # esp-tflite-micro's MicroMutableOpResolver (the exporter reads them from its headers).
    idf_component_get_property(CNN_TFLM_DIR espressif__esp-tflite-micro COMPONENT_DIR)
    if (NOT CNN_TFLM_DIR)
        message(FATAL_ERROR "cnn: esp-tflite-micro component not found (see idf_component.yml)")
    endif ()
    set(CNN_TFLM_HEADERS "${CNN_TFLM_DIR}/tensorflow/lite/micro/micro_mutable_op_resolver.h"
            "${CNN_TFLM_DIR}/tensorflow/lite/schema/schema_generated.h")

    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(CNN_MODEL_GEN_CMD ${Python3_EXECUTABLE} "${CNN_MODEL_EXPORT}"
            --model "${CNN_MODEL_TFLITE}"
            --tflm-dir "${CNN_TFLM_DIR}"
            --out "${CMAKE_CURRENT_BINARY_DIR}/model_data")
    # Also generate once at configure time, so the header exists as soon as the project is
    # loaded (IDEs like CLion resolve includes after a CMake reload, before any build).
    execute_process(COMMAND ${CNN_MODEL_GEN_CMD} OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
    # Regenerates on build whenever the model, the labels, the exporter or the tflm headers change.
    add_custom_command(
            OUTPUT ${CNN_MODEL_OUT}
            COMMAND ${CNN_MODEL_GEN_CMD}
            DEPENDS "${CNN_MODEL_TFLITE}" "${CNN_MODEL_LABELS}" "${CNN_MODEL_EXPORT}" ${CNN_TFLM_HEADERS}
            COMMENT "Generating model_data.h/.cc from fire_smoke_cnn_int8.tflite"
    )
    add_custom_target(cnn_model_data DEPENDS ${CNN_MODEL_OUT})
    add_dependencies(${COMPONENT_LIB} cnn_model_data)
endif ()