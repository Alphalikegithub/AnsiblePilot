#!/bin/bash
# ==============================================================================
# 脚本名称: install_ansible_offline.sh
# 作用: 在无网的麒麟V10（ARM64）目标机器上执行，一键部署离线提取的 Ansible 环境
# ==============================================================================

set -e

PACKAGE_TAR="ansible_arm64_kylin_offline.tar.gz"
WORK_DIR="./ansible_offline_package_arm64"

echo "=================================================================="
echo ">>> 开始在当前机器上安装/部署 Ansible 离线运行环境..."
echo ">>> 目标环境架构: $(uname -m), 用户: $(whoami)"
echo "=================================================================="

# 1. 检查压缩包或解压目录
if [ ! -d "${WORK_DIR}" ]; then
    if [ -f "${PACKAGE_TAR}" ]; then
        echo "[*] 正在解压离线包 ${PACKAGE_TAR} ..."
        tar -xzvf "${PACKAGE_TAR}"
    else
        echo "[-] 错误: 未找到 ${PACKAGE_TAR} 或 ${WORK_DIR} 目录！"
        echo "    请将提取脚本生成的压缩包放置在当前目录下。"
        exit 1
    fi
fi

# 2. 检查当前系统的 Python 3 环境
PYTHON_BIN=$(which python3 2>/dev/null || true)
if [ -z "$PYTHON_BIN" ]; then
    echo "[-] 错误: 当前系统未找到 python3。麒麟V10系统默认自带 Python3，请检查 /usr/bin/python3 是否存在。"
    exit 1
fi
echo "[+] 找到系统 Python3 路径: ${PYTHON_BIN} ($(${PYTHON_BIN} --version))"

# 3. 部署 Python 模块到系统路径
TARGET_SITE_DIR=$($PYTHON_BIN -c "import site; print(site.getsitepackages()[0])" 2>/dev/null || echo "/usr/local/lib/python3/dist-packages")
if [ ! -d "$TARGET_SITE_DIR" ]; then
    TARGET_SITE_DIR="/usr/lib/python3/dist-packages"
fi

echo "[*] 正在部署 Python 模块到: ${TARGET_SITE_DIR} ..."

if [ -d "${WORK_DIR}/python_modules" ]; then
    if [ "$(id -u)" -eq 0 ]; then
        cp -rn "${WORK_DIR}/python_modules/"* "${TARGET_SITE_DIR}/" 2>/dev/null || true
    else
        # 非 root 用户安装到用户目录
        USER_SITE_DIR=$($PYTHON_BIN -c "import site; print(site.getusersitepackages())" 2>/dev/null || echo "$HOME/.local/lib/python3.8/site-packages")
        mkdir -p "$USER_SITE_DIR"
        echo "[!] 检测到当前非 root 用户，部署到用户目录: ${USER_SITE_DIR}"
        cp -rn "${WORK_DIR}/python_modules/"* "${USER_SITE_DIR}/" 2>/dev/null || true
    fi
fi

# 4. 部署可执行二进制文件与命令
echo "[*] 正在安装可执行命令到 /usr/local/bin (或 ~/.local/bin) ..."
BIN_DEST="/usr/local/bin"
if [ "$(id -u)" -ne 0 ]; then
    BIN_DEST="$HOME/.local/bin"
    mkdir -p "$BIN_DEST"
    # 确保 PATH 包含 ~/.local/bin
    if [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
        export PATH="$HOME/.local/bin:$PATH"
        echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$HOME/.bashrc"
    fi
fi

if [ -d "${WORK_DIR}/bin" ]; then
    for f in "${WORK_DIR}/bin/"*; do
        if [ -f "$f" ]; then
            fname=$(basename "$f")
            cp -f "$f" "${BIN_DEST}/${fname}"
            chmod +x "${BIN_DEST}/${fname}"
            echo "[+] 安装命令: ${BIN_DEST}/${fname}"
        fi
    done
fi

# 5. 安装可能存在的动态共享库
if [ -d "${WORK_DIR}/libs" ] && [ "$(ls -A ${WORK_DIR}/libs 2>/dev/null)" ]; then
    echo "[*] 正在补充动态链接库..."
    if [ "$(id -u)" -eq 0 ]; then
        cp -n "${WORK_DIR}/libs/"* /usr/lib/ 2>/dev/null || true
        ldconfig 2>/dev/null || true
    fi
fi

# 6. 配置 /etc/ansible 基础目录
if [ "$(id -u)" -eq 0 ]; then
    mkdir -p /etc/ansible
    if [ ! -f /etc/ansible/ansible.cfg ]; then
        cat << 'EOF' > /etc/ansible/ansible.cfg
[defaults]
host_key_checking = False
timeout = 10
deprecation_warnings = False
EOF
        echo "[+] 已创建基础 /etc/ansible/ansible.cfg"
    fi
fi

# 7. 最终验证与测试
echo ""
echo "=================================================================="
echo ">>> 正在验证 Ansible 命令执行状态..."
echo "=================================================================="

if command -v ansible &>/dev/null; then
    echo "[✓] ansible 命令正常可用:"
    ansible --version
    echo ""
    echo "[✓] 离线部署成功！现在可以在无网环境下直接启动 AnsiblePilot 工具进行图形化运维。"
else
    echo "[-] 未在 PATH 中找到 ansible，请检查环境变量 PATH 或查看 ${BIN_DEST}/ansible 是否存在。"
    exit 1
fi
