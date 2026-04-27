# Idea

## Grundidee
Ein autonomes Segelflugzeug, welches vordefinierte Flächen, beispielsweise Felder, abfliegt und Fotografiert. Über die Bilder können dann beispielsweise Ernteerträge geschätzt, Unkraut erkannt oder anderweitiger Handlungsbedarf erkannt werden. Das Flugzeug soll dabei im Notfall über eine Fernsteuerung manuell gesteuert werden können.

## Kommunikation, Konfiguration und Navigation
Das Flugzeug kommuniziert über LoRa mit einer Basisstation, welche über einen Hotspot und eine Webseite die Konfiguration der Flugfläche und weitere Daten ermöglicht. Die Webseite soll auch die Möglichkeit bieten einen Preview der bereits geflogenen Bilder anzuzeigen. Die Bilder werden dabei auf einer SD-Karte gespeichert und können dann manuell von dort abgerufen werden.

Mittels Beschleunigungssensor und Barometer soll die Höhe und Lage des Flugzeugs bestimmt werden können. Über GPS kann dann die korrekte Position und die vollständige automatische Abdeckung der Fläche gewährleistet werden.

## Start und Landung
Für den Start soll das Flugzeug von Hand geworfen werden können und sofort mit der automatischen Navigation beginnen. Für die Landung soll das Flugzeug zum Startpunkt zurückkehren und entweder manuell gelandet, gefangen oder mittels eines genauen Laser Entfernungsmessers versuchen vorsichtig auf einer Wiese zu landen.