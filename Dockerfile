FROM debian:bookworm-slim AS build

# Prevent initramfs and ldconfig runs in CI, speeds up the build
RUN dpkg-divert --local --rename --add /usr/sbin/update-initramfs \
    && dpkg-divert --local --rename --add /sbin/ldconfig \
    && dpkg-divert --local --rename --add /usr/sbin/ldconfig \
    && ln -sf /bin/true /usr/sbin/update-initramfs \
    && ln -sf /bin/true /sbin/ldconfig \
    && ln -sf /bin/true /usr/sbin/ldconfig

# Install build dependencies
# eatmydata ignores fsck calls which arent needed in CI, speeds up the build
RUN apt update \
    && apt install -o APT::Install-Suggests=false -y eatmydata \
    && eatmydata apt install -o APT::Install-Suggests=false -y \
                gcc \
                meson \
                libsocketcan-dev \
                libconfig-dev

# Build and strip
WORKDIR /src
COPY . .
RUN meson setup -Dlibconfig=true --buildtype=release build \
    && meson compile -C build \
    && meson install -C build

# Collect only needed runtime files (arch-independent)
RUN mkdir -p /minimal-root/usr/local/sbin /minimal-root/lib /minimal-root/lib64 /minimal-root/etc \
    # The binary
    && cp /usr/local/sbin/socketcand /minimal-root/usr/local/sbin/ \
    # Get interpreter from ELF header and copy
    && interp=$(readelf -l /usr/local/sbin/socketcand | awk -F ': ' '/interpreter/ {print $2}' | tr -d ']') \
    && mkdir -p "/minimal-root$(dirname $interp)" \
    && cp "$interp" "/minimal-root$interp" \
    # Copy linked shared libraries
    && ldd /usr/local/sbin/socketcand | awk '{print $3}' | grep '^/' | sort -u | xargs -I '{}' cp -v --parents '{}' /minimal-root/ \
    # Minimal /etc
    && cp -a /etc/nsswitch.conf /minimal-root/etc/ \
    && cp -a /etc/hosts /minimal-root/etc/ \
    && cp -a /etc/resolv.conf /minimal-root/etc/

# Build minimal image
FROM scratch
COPY --from=build /minimal-root/ /
ENTRYPOINT ["/usr/local/sbin/socketcand"]
