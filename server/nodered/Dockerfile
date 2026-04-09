FROM nodered/node-red:latest

USER root

COPY nodered/install-deps.sh /tmp/install-deps.sh
RUN chmod +x /tmp/install-deps.sh && /tmp/install-deps.sh && rm /tmp/install-deps.sh

COPY nodered/package.json /data/package.json
RUN cd /data && npm install --unsafe-perm --no-update-notifier --no-fund --omit=dev

COPY nodered/settings.js /data/settings.js
COPY nodered/package.json /opt/smarthome/package.json
COPY nodered/settings.js /opt/smarthome/settings.js
COPY nodered/build-flows.js /opt/smarthome/build-flows.js
COPY nodered/entrypoint.sh /opt/smarthome/entrypoint.sh
COPY nodered/flows /opt/smarthome/flows
COPY nodered/lib /opt/smarthome/lib
COPY sqlite/00_schema_phase1.sql /opt/smarthome/schema.sql

RUN node /opt/smarthome/build-flows.js > /data/flows.json \
    && chmod +x /opt/smarthome/entrypoint.sh \
    && mkdir -p /data/sqlite \
    && chown -R node-red:root /data /opt/smarthome

USER node-red

ENTRYPOINT ["/opt/smarthome/entrypoint.sh"]
