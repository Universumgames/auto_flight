build_motor_controller:
	cd motor_controller && pio run 

build_flight_controller:
	cd flight_controller && pio run

build_base_station:
	cd base_station && pio run

build_3d_models:
	cd 3d_models && $(MAKE) all

build_all: build_motor_controller build_flight_controller build_base_station build_3d_models

clean_motor_controller:
	cd motor_controller && pio run --target clean

clean_flight_controller:
	cd flight_controller && pio run --target clean

clean_base_station:
	cd base_station && pio run --target clean

clean_3d_models:
	cd 3d_models && $(MAKE) clean

clean_all: clean_motor_controller clean_flight_controller clean_base_station clean_3d_models

# Native (host) GoogleTest suite - see flight_controller/test/README.md.
# Mirrors the flight_controller_test CI job, including the JUnit-compatible
# XML report GitLab picks up via `artifacts.reports.junit`.
test_flight_controller:
	cd flight_controller && cmake -S . -B build_native -DCMAKE_BUILD_TYPE=Debug
	cd flight_controller && cmake --build build_native -j
	cd flight_controller && ./build_native/flight_controller_test --gtest_output=xml:build_native/report.xml

test_all: test_flight_controller

clean_flight_controller_test:
	rm -rf flight_controller/build_native

create_links:
	cd flight_controller && rm -f motor_controller
	cd flight_controller && ln -s ../shared_components shared_components
	cd base_station && rm -f shared_components
	cd base_station && ln -s ../shared_components shared_components

render_architecture_diagram:
	cd assets && xelatex -interaction=nonstopmode -halt-on-error architecture.tex
	pdftocairo -svg assets/architecture.pdf assets/architecture.svg
	rm -f assets/architecture.aux assets/architecture.log assets/architecture.pdf

export_documentation: render_architecture_diagram
	pandoc -s -f markdown -t pdf -o README.pdf \
		README.md \
		--toc --number-sections \
        -V lang=de-DE \
        -V breakurl -V hyphens=URL -V colorlinks \
        -V geometry=a4paper,left=3cm,right=2cm,top=2cm,bottom=2cm
	pandoc -s -f markdown -t pdf -o README_de.pdf \
		README_de.md \
		--toc --number-sections \
        -V lang=de-DE \
        -V breakurl -V hyphens=URL -V colorlinks \
        -V geometry=a4paper,left=3cm,right=2cm,top=2cm,bottom=2cm

cicd_local_test:
	gitlab-ci-local --concurrency 1 --stage test