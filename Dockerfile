FROM alpine:latest AS build-env
RUN apk add -qU alpine-sdk jansson-dev libmicrohttpd-dev libressl-dev

# build
ADD . /tmp/brubeck
WORKDIR /tmp/brubeck
RUN ./script/cibuild


# final stage
FROM alpine
RUN apk add --no-cache jansson libmicrohttpd libressl
WORKDIR /usr/sbin/
COPY --from=build-env /tmp/brubeck/brubeck /usr/sbin/
COPY --from=build-env /tmp/brubeck/debian/biz.json /tmp/brubeck/debian/tech.json /etc/brubeck/
