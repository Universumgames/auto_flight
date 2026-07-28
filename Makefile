build_motor_controller:
	cd motor_controller && pio run 

build_flight_controller:
	cd flight_controller && pio run

build_base_station:
	cd base_station && pio run

build_all: build_motor_controller build_flight_controller build_base_station

clean_motor_controller:
	cd motor_controller && pio run --target clean

clean_flight_controller:
	cd flight_controller && pio run --target clean

clean_base_station:
	cd base_station && pio run --target clean

clean_all: clean_motor_controller clean_flight_controller clean_base_station

create_links:
	cd flight_controller && rm -f motor_controller
	cd flight_controller && ln -s ../shared_components shared_components
	cd base_station && rm -f shared_components
	cd base_station && ln -s ../shared_components shared_components

readme_pdf:
	pandoc README.md -o README.pdf