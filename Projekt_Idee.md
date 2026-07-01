# Initiale Idea

## Grundidee
Ein autonomes Segelflugzeug, welches vordefinierte Flächen, beispielsweise Felder, abfliegt und Fotografiert. Über die Bilder können dann beispielsweise Ernteerträge geschätzt, Unkraut erkannt oder anderweitiger Handlungsbedarf erkannt werden. Das Flugzeug soll dabei im Notfall über eine Fernsteuerung manuell gesteuert werden können.

## Kommunikation, Konfiguration und Navigation
Das Flugzeug kommuniziert über LoRa mit einer Basisstation, welche über einen Hotspot, eine Webseite bereitstellt, die die Konfiguration der Flugfläche und weiterer Daten ermöglicht. 

Mittels Beschleunigungssensor, Barometer und Magnetometer soll die Höhe und Lage des Flugzeugs bestimmt werden können. Über GPS kann die korrekte Position ermöglicht werden und soll so das automatische Abfliegen der berechneten Waypoints gewährleisten.


## Mögliche Verwendung
Mit der Erweiterung des Systems durch eine Kamera, können Luftbilder aufgenommen werden, die dann für die Analyse von landwirtschaftlichen Flächen verwendet werden können.

Die Kommunikation über LoRa ermöglicht eine große Reichweite, jedoch eine geringe Bandbreite. Daher ist es notwendig, die Datenmenge, die über LoRa übertragen wird, zu minimieren. Die Bilder können daher auf einer SD-Karte gespeichert werden und nur die relevanten Daten, wie GPS-Koordinaten und Statusinformationen, werden über LoRa übertragen. Alternativ zur Speicherung auf einer SD-Karte, die manuell ausgelesen werden müsste, könnte das Flugzeug bei der Landung die Bilder über WLAN an die Basisstation übertragen. Dies würde eine schnellere und einfachere Auswertung der Bilder ermöglichen, da sie direkt auf dem Computer der Basisstation verfügbar wären.

## Start und Landung
Für den Start soll das Flugzeug von Hand geworfen werden können und sofort mit der automatischen Navigation beginnen. Für die Landung soll das Flugzeug zum Startpunkt zurückkehren und entweder manuell gelandet, gefangen oder mittels eines genauen Laser Entfernungsmessers versuchen vorsichtig auf einer Wiese zu landen.