# .ide/Dockerfile

# Use the default development environment as a base image
FROM cnbcool/default-dev-env:latest

# non-interactive apt
ARG DEBIAN_FRONTEND=noninteractive

# set cargo environment
ENV CARGO_HOME=/root/.cargo
ENV PATH=${CARGO_HOME}/bin:${PATH}

# copy claude config
COPY CLAUDE.md /root/.claude/CLAUDE.md

# install vscode and extension
RUN code-server --install-extension eamodio.gitlens

# install nodejs, claude-code and mcp tools
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash -; \
    apt-get install -y nodejs; \
    npm install -g @anthropic-ai/claude-code;

# install qemu build dependencies, rust, bindgen, mutt
RUN sed -i 's/^Types: deb$/Types: deb deb-src/' \
        /etc/apt/sources.list.d/debian.sources; \
    apt update; \
    apt build-dep -y qemu; \
    apt install -y python3-venv; \
    apt install -y libclang-dev clang llvm-dev; \
    apt install mutt; \
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y; \
    rustc --version; \
    cargo --version; \
    cargo install --locked bindgen-cli; \
    rm -rf /var/lib/apt/lists/*; \
    rm -rf /root/.cargo/registry /root/.cargo/git

# install rt-claw build dependencies (meson, ARM toolchain, QEMU ARM)
RUN apt update && apt install -y \
        meson ninja-build \
        gcc-arm-none-eabi \
        qemu-system-arm \
        scons \
    && rm -rf /var/lib/apt/lists/*

# install ESP-IDF v5.4 + toolchains + Espressif QEMU
ENV IDF_PATH=/root/esp/esp-idf
RUN git clone --depth 1 --branch v5.4 \
        --recursive --shallow-submodules \
        https://github.com/espressif/esp-idf.git ${IDF_PATH} \
    && cd ${IDF_PATH} \
    && ./install.sh esp32c3,esp32s3 \
    && python3 tools/idf_tools.py install qemu-riscv32 \
    && python3 tools/idf_tools.py install qemu-xtensa \
    && rm -rf /root/.espressif/dist

ENV LANG C.UTF-8
ENV LANGUAGE C.UTF-8
