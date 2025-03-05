FROM apache/kafka:3.9.0
COPY --chown=appuser server.properties /opt/kafka/config/kraft/server.properties
COPY --chown=appuser server.properties /opt/kafka/config/server.properties
COPY --chown=appuser server_keystore.jks /usr/local/etc/kafka/certs/server_keystore.jks
COPY --chown=appuser server_truststore.pem /usr/local/etc/kafka/certs/server_truststore.pem
