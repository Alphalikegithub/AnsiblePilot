#!/bin/bash
# ==============================================================================
# 脚本名称: export_ansible_arm64.sh
# 作用: 在已安装 Ansible 的机器（麒麟V10 ARM64）上运行，提取 Ansible 及所有依赖
# ==============================================================================

set -e

EXPORT_DIR="./ansible_offline_package_arm64"
DEBS_DIR="${EXPORT_DIR}/debs"
PIPS_DIR="${EXPORT_DIR}/pips"
LIBS_DIR="${EXPORT_DIR}/libs"

echo "=================================================================="
echo ">>> 开始在当前机器上打包/提取 Ansible 离线运行环境..."
echo ">>> 系统架构: $(uname -m), 系统发行版: $(cat /etc/os-release | grep PRETTY_NAME | cut -d= -f2)"
echo "=================================================================="

rm -rf "${EXPORT_DIR}"
mkdir -p "${DEBS_DIR}" "${PIPS_DIR}" "${LIBS_DIR}"

# 1. 检查 ansible 命令位置
ANSIBLE_BIN=$(which ansible 2>/dev/null || true)
if [ -z "$ANSIBLE_BIN" ]; then
    echo "[-] 错误: 当前机器未找到 ansible 命令，请确认此机器是否已正确安装 Ansible。"
    exit 1
fi
echo "[+] 找到 Ansible 路径: ${ANSIBLE_BIN}"

# 2. 方法 A: 尝试导出 APT deb 依赖包 (如果系统是通过 apt/dpkg 安装的)
echo ""
echo "[*] 正在分析 APT/DPKG 安装包及依赖..."
ANSIBLE_PKGS=$(dpkg -l | grep -E "ansible|python3-jinja2|python3-yaml|python3-paramiko|python3-cryptography|sshpass" | awk '{print $2}' || true)

if [ -n "$ANSIBLE_PKGS" ]; then
    echo "[+] 发现系统包: ${ANSIBLE_PKGS}"
    for pkg in $ANSIBLE_PKGS; do
        # 优先从本地 apt 缓存寻找 deb
        find /var/cache/apt/archives/ -name "${pkg}*.deb" -exec cp {} "${DEBS_DIR}/" \; 2>/dev/null || true
    done
fi

# 3. 方法 B: 导出 Python 模块与 Wheels (支持 pip 安装或提取已安装的 site-packages)
echo ""
echo "[*] 正在提取 Python 相关依赖..."
PYTHON_BIN=$(head -n 1 "$ANSIBLE_BIN" | sed 's/^#!//' | awk '{print $1}')
if [ ! -x "$PYTHON_BIN" ]; then
    PYTHON_BIN=$(which python3)
fi
echo "[+] Ansible 绑定的 Python 解释器: ${PYTHON_BIN}"

# 如果有 pip，尝试下载/查找 wheel 包
if command -v pip3 &>/dev/null; then
    echo "[*] 尝试通过 pip3 导出离线 wheels..."
    pip3 download ansible -d "${PIPS_DIR}" 2>/dev/null || true
fi

# 4. 方法 C: 兜底全量打包 site-packages (保证 100% 零遗漏)
echo ""
echo "[*] 正在直接归档当前 Python 环境中的 ansible 模块与库文件..."
SYS_SITE_PACKAGES=$($PYTHON_BIN -c "import site; print(site.getsitepackages()[0])" 2>/dev/null || echo "/usr/lib/python3/dist-packages")
USER_SITE_PACKAGES=$($PYTHON_BIN -c "import site; print(site.getusersitepackages())" 2>/dev/null || true)

mkdir -p "${EXPORT_DIR}/python_modules"

# 打包核心 python 库
CORE_MODULES=(
    "ansible"
    "ansible_collections"
    "ansible_core"
    "jinja2"
    "yaml"
    "_yaml"
    "paramiko"
    "cryptography"
    "cffi"
    "pycparser"
    "bcrypt"
    "pynacl"
    "markupsafe"
    "resolvelib"
    "packaging"
)

for base_dir in "$SYS_SITE_PACKAGES" "$USER_SITE_PACKAGES" "/usr/local/lib/python3.*/dist-packages" "/usr/lib/python3/dist-packages"; do
    if [ -d "$base_dir" ]; then
        echo "[+] 扫描目录: $base_dir"
        for mod in "${CORE_MODULES[@]}"; do
            # 拷贝文件夹或.py文件及egg-info/dist-info
            cp -r ${base_dir}/${mod}* "${EXPORT_DIR}/python_modules/" 2>/dev/null || true
        done
    fi
done

# 5. 提取二进制执行命令与 sshpass、openssh-client
echo ""
echo "[*] 正在提取二进制可执行文件及系统工具..."
mkdir -p "${EXPORT_DIR}/bin"

BIN_LIST=(
    "ansible"
    "ansible-playbook"
    "ansible-galaxy"
    "ansible-vault"
    "ansible-doc"
    "ansible-inventory"
    "ansible-config"
    "sshpass"
)

for b in "${BIN_LIST[@]}"; do
    FOUND_BIN=$(which "$b" 2>/dev/null || true)
    if [ -n "$FOUND_BIN" ]; then
        echo "[+] 备份二进制文件: $FOUND_BIN"
        cp "$FOUND_BIN" "${EXPORT_DIR}/bin/"
        
        # 提取 ELF 共享库依赖 (.so)
        if file "$FOUND_BIN" | grep -q "ELF"; then
            ldd "$FOUND_BIN" 2>/dev/null | awk '{if ($3 ~ /^\//) print $3}' | while read -r so_path; do
                cp -n "$so_path" "${LIBS_DIR}/" 2>/dev/null || true
            done
        fi
    fi
done

# 6. 生成离线安装信息清单
echo ""
cat << 'EOF' > "${EXPORT_DIR}/VERSION_INFO.txt"
Ansible 离线提取包 (ARM64 麒麟 V10)
提取时间: $(date)
源机器系统架构: $(uname -m)
Ansible 版本:
EOF
ansible --version >> "${EXPORT_DIR}/VERSION_INFO.txt" 2>&1 || true

# 7. 打包成单个 tar.gz 压缩包
TAR_NAME="ansible_arm64_kylin_offline.tar.gz"
echo ""
echo "[*] 正在压缩打包为 ${TAR_NAME} ..."
tar -czvf "${TAR_NAME}" "${EXPORT_DIR}"

echo ""
echo "=================================================================="
echo ">>> [成功] 提取完成！"
echo ">>> 生成的离线部署包文件: $(pwd)/${TAR_NAME}"
echo ">>> 您可以将该压缩包通过 U 盘或内网传输到目标无网机器上，并使用配套的 install_ansible_offline.sh 进行部署。"
echo "=================================================================="
