ARG GO_IMAGE=golang:1.27.0-bookworm
ARG MKPSXISO_VERSION=2.30

FROM ${GO_IMAGE} AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG MKPSXISO_VERSION
ARG MKPSXISO_COMMIT=54fb1644ed8741223583e2dcda358b75a205e214
ARG OLD_GCC_RELEASE=0.17
ARG GCC_263_PSX_SHA256=01e6e8c4933414ea3f8d8e3bc766a1f5fafd4fc0110e0b75d1f691bd791989b1
ARG GCC_272_PSX_SHA256=500a459b3485e885a8d302cac23c2a4632f3900e03a09153f6190699fd723571
ARG MASPSX_COMMIT=7686f845a181700534c83c0419183e38aeb3e49c
ARG ASM_DIFFER_COMMIT=c80ea4e3d16ecc8f4c21923633f3f9b1fb534c53
ARG DECOMP_PERMUTER_COMMIT=fb516c435c6f362fbced66e171545324306b607b

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        git \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

RUN git clone \
        --branch "v${MKPSXISO_VERSION}" \
        --depth 1 \
        --recurse-submodules \
        --shallow-submodules \
        https://github.com/Lameguy64/mkpsxiso.git /src/mkpsxiso \
    && test "$(git -C /src/mkpsxiso rev-parse HEAD)" = "${MKPSXISO_COMMIT}" \
    && cmake --preset release -DMKPSXISO_NO_LIBFLAC=1 -S /src/mkpsxiso \
    && cmake --build /src/mkpsxiso/build --parallel \
    && cmake --install /src/mkpsxiso/build --prefix /opt/mkpsxiso

RUN mkdir -p /opt/old-gcc/2.6.3-psx /opt/old-gcc/2.7.2-psx \
    && curl --fail --location --output /tmp/gcc-2.6.3-psx.tar.gz \
        "https://github.com/decompals/old-gcc/releases/download/${OLD_GCC_RELEASE}/gcc-2.6.3-psx.tar.gz" \
    && echo "${GCC_263_PSX_SHA256}  /tmp/gcc-2.6.3-psx.tar.gz" | sha256sum --check --strict \
    && tar -xzf /tmp/gcc-2.6.3-psx.tar.gz -C /opt/old-gcc/2.6.3-psx \
    && curl --fail --location --output /tmp/gcc-2.7.2-psx.tar.gz \
        "https://github.com/decompals/old-gcc/releases/download/${OLD_GCC_RELEASE}/gcc-2.7.2-psx.tar.gz" \
    && echo "${GCC_272_PSX_SHA256}  /tmp/gcc-2.7.2-psx.tar.gz" | sha256sum --check --strict \
    && tar -xzf /tmp/gcc-2.7.2-psx.tar.gz -C /opt/old-gcc/2.7.2-psx

# asm-differ renders the `make diff` listing of a mismatching function.
RUN fetch() { \
        git clone "$1" "$2" \
        && git -C "$2" checkout "$3" \
        && test "$(git -C "$2" rev-parse HEAD)" = "$3" \
        && rm -rf "$2/.git"; \
    } \
    && fetch https://github.com/mkst/maspsx.git /opt/maspsx "${MASPSX_COMMIT}" \
    && fetch https://github.com/simonlindholm/asm-differ.git /opt/asm-differ "${ASM_DIFFER_COMMIT}" \
    && fetch https://github.com/simonlindholm/decomp-permuter.git /opt/decomp-permuter "${DECOMP_PERMUTER_COMMIT}"

FROM ${GO_IMAGE}

ARG DEBIAN_FRONTEND=noninteractive
ARG MKPSXISO_VERSION
ARG MIPS_CPP_VERSION=12.2.0
ARG MIPS_BINUTILS_VERSION=2.40
ARG CLANG_FORMAT_VERSION=22.1.8

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        binutils-mipsel-linux-gnu \
        ca-certificates \
        cpp-mipsel-linux-gnu \
        python3 \
        python3-venv \
        qemu-user \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/mkpsxiso/bin/ /usr/local/bin/
COPY --from=builder /opt/old-gcc /opt/old-gcc
COPY --from=builder /opt/maspsx /opt/maspsx
COPY --from=builder /opt/asm-differ /opt/asm-differ
COPY --from=builder /opt/decomp-permuter /opt/decomp-permuter

RUN python3 -m venv /opt/decomp-tools \
    && /opt/decomp-tools/bin/pip install --no-cache-dir --only-binary=:all: \
        "clang-format==${CLANG_FORMAT_VERSION}" \
        colorama==0.4.6 \
        Levenshtein==0.27.1 \
        rapidfuzz==3.14.6 \
        toml==0.10.2 \
        watchdog==6.0.0 \
    && ln -s /opt/asm-differ/diff.py /opt/decomp-tools/bin/asm-differ \
    && ln -s /opt/decomp-permuter/permuter.py /opt/decomp-tools/bin/decomp-permuter \
    && ln -s /opt/decomp-permuter/import.py /opt/decomp-tools/bin/decomp-permuter-import

COPY tools/go.mod tools/go.sum /opt/tools/
RUN cd /opt/tools && go mod download && chmod -R a+rwX /go/pkg/mod

# Containers run as the host user with the checkout at /work: HOME must be
# writable, the Go build cache persists in the checkout's build/, and builds
# skip VCS stamping so a checkout mid-rebase still builds.
ENV HOME=/tmp
ENV GOCACHE=/work/build/cache/go
ENV GOFLAGS=-buildvcs=false
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV PATH="/opt/decomp-tools/bin:${PATH}"

# Verify the toolchain as the build runs it: each cc1 under qemu-i386, then
# maspsx, as, ld and objdump, plus the pinned versions of everything else.
RUN printf 'int f(int x) { return x / 3; }\n' >/tmp/t.c \
    && for cc1 in /opt/old-gcc/2.6.3-psx/cc1 /opt/old-gcc/2.7.2-psx/cc1; do \
        qemu-i386 "$cc1" -quiet -O2 -G0 -mcpu=3000 -msoft-float -mgas -fgnu-linker /tmp/t.c -o /tmp/t.s \
        && python3 /opt/maspsx/maspsx.py --aspsx-version=2.34 </tmp/t.s >/tmp/t.maspsx.s \
        && mipsel-linux-gnu-as -EL -march=r3000 -G0 -o /tmp/t.o /tmp/t.maspsx.s \
        && mipsel-linux-gnu-ld -EL -r -o /tmp/t.elf /tmp/t.o \
        && mipsel-linux-gnu-objdump -d /tmp/t.elf | grep --quiet '<f>:' \
        || exit 1; \
    done \
    && rm /tmp/t.* \
    && test "$(mipsel-linux-gnu-cpp -dumpfullversion)" = "${MIPS_CPP_VERSION}" \
    && mipsel-linux-gnu-as --version | grep --quiet --fixed-strings " ${MIPS_BINUTILS_VERSION}" \
    && mipsel-linux-gnu-objcopy --version >/dev/null \
    && mkpsxiso --help | grep --quiet --fixed-strings "MKPSXISO ${MKPSXISO_VERSION}" \
    && dumpsxiso --help | grep --quiet --fixed-strings "DUMPSXISO ${MKPSXISO_VERSION}" \
    && clang-format --version | grep --quiet --fixed-strings " ${CLANG_FORMAT_VERSION}" \
    && decomp-permuter --help >/dev/null \
    && (cd /opt/asm-differ && python3 -c 'import diff') \
    && go version

WORKDIR /work
CMD ["/bin/bash"]
