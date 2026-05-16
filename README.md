# AnsiblePilot 使用说明

`AnsiblePilot` 是一个基于 Qt/C++ 的 Ansible 可视化批量运维工具，用来把常用的 Ansible 操作做成图形界面，方便不会频繁敲命令的人快速执行批量上传、批量部署、批量命令和连通性测试。

## 1. 功能概览

当前程序支持以下功能：

- **自动扫描 Ansible 文件**：扫描工作目录下的 `*.yaml`、`*.yml`、`hosts`、`my_hosts`、`*.ini`。
- **批量部署**：选择已有 playbook，例如 `ansible.yaml`、`ansible-start.yaml`、`ansible-redeploy.yaml` 后执行。
- **Ping 测试**：调用 `ansible -m ping` 检查目标主机连通性。
- **批量执行命令**：通过 `shell`、`command`、`service`、`systemd` 等模块批量执行命令。
- **批量上传**：通过 Ansible `copy` 模块把本地文件或目录上传到远端服务器。
- **常用操作**：服务管理、系统信息、进程与日志、文件操作、软件包等可视化按钮，无需手写 playbook。
- **外部终端执行**：通过系统默认终端运行 Ansible，避免 GUI 进程环境限制影响 Ansible。
- **执行日志同步**：外部终端执行时，同步显示 Ansible 标准输出、错误输出和退出码。
- **多进程残留清理**：每次执行前自动检查 `/dev/shm` 中 Python multiprocessing 残留信号量并清理，避免重复执行后 `Errno 13` 报错。
- **命令预览**：执行前展示即将执行的命令。

## 2. 编译方式

### 2.1 使用 Qt Creator 编译

推荐使用 Qt Creator 打开项目。

步骤如下：

1. 打开 Qt Creator。
2. 选择 `File -> Open File or Project`。
3. 打开项目文件：

   ```text
   AnsiblePilot.pro
   ```

4. 选择可用的 Qt Kit。
5. 点击 `Build -> Run qmake`。
6. 点击 `Build -> Build Project`。
7. 编译成功后点击运行。

### 2.2 命令行编译

如果你的系统已经配置好 `qmake` 和编译器，可以在项目目录执行：

```bash
qmake AnsiblePilot.pro
make
```

## 3. 运行前准备

### 3.1 安装 Ansible

程序本身只是图形界面，真正执行任务仍然依赖系统中的 `ansible` 和 `ansible-playbook` 命令。

确认 Linux 系统中命令可用：

```bash
ansible --version
ansible-playbook --version
```

### 3.2 准备系统终端

程序执行 Ansible 时会优先使用系统默认终端。

建议确认默认终端可用：

```bash
x-terminal-emulator --version
```

也可以查看系统默认终端：

```bash
update-alternatives --list x-terminal-emulator
```

程序会依次尝试：

```text
x-terminal-emulator
mate-terminal
gnome-terminal
konsole
xfce4-terminal
xterm
```

### 3.3 准备 SSH 访问

Ansible 批量操作依赖 SSH 登录远端服务器。

请确保：

- 当前 Linux 主机能 SSH 到目标服务器。
- Inventory 文件中的用户配置正确，例如：

  ```ini
  [web]
  172.10.10.11 ansible_ssh_user=root
  172.10.10.12 ansible_ssh_user=root
  ```

- 如果没有配置免密登录，可以在界面的“额外参数”里填写：

  ```text
  --ask-pass
  ```

- 如果需要 sudo/become 密码，可以填写：

  ```text
  --ask-become-pass
  ```

## 4. 界面区域说明

程序主界面分为三部分：

### 4.1 基础配置

#### 工作目录

Ansible 项目的根目录。

例如：

```text
/home/user/ansible
```

点击 `选择目录` 可以切换目录。

切换后点击 `刷新扫描`，程序会重新扫描 playbook 和 inventory 文件。

#### Inventory

Ansible 主机清单文件。

常见文件名：

```text
hosts
my_hosts
```

程序会自动扫描，也可以手动点击 `选择 Inventory`。

如果系统存在：

```text
/etc/ansible/hosts
```

程序启动时会优先加入并默认选择这个 Inventory。工作目录中的 `hosts`、`my_hosts`、`*.ini` 也会同时加入下拉框。

#### 主机组

要执行的 Ansible 主机组。

例如：

```text
web
all
```

如果你的 inventory 是：

```ini
[web]
172.10.10.11 ansible_ssh_user=root
```

这里就填写：

```text
web
```

#### Limit

限制只对部分主机执行。

例如只执行某台机器：

```text
172.10.10.11
```

或者限制某个组：

```text
web
```

等价于命令行参数：

```bash
--limit 172.10.10.11
```

#### 额外参数

填写额外的 Ansible 参数。

常用示例：

```text
-vv
```

```text
--ask-pass
```

```text
--ask-become-pass
```

```text
--check
```

也可以组合：

```text
-vv --ask-pass --ask-become-pass
```

## 5. 常用操作（推荐入口）

如果你不熟悉 playbook，请优先使用 **常用操作** 标签页。所有按钮内部都会自动转换为标准 Ansible ad-hoc 命令，命令本身会同步显示在日志区域，方便你对照学习。

### 5.1 服务管理

- 输入服务名（如 `sshd`、`nginx`）。
- 点击 **启动服务** / **停止服务** / **重启服务** / **查看状态**。

等价命令：

```bash
ansible -i hosts web -m systemd -a "name=sshd state=restarted"
ansible -i hosts web -m shell   -a "systemctl status sshd --no-pager"
```

### 5.2 系统信息

一键查看：

- 磁盘 `df -h`
- 内存 `free -h`
- 运行时间 `uptime`
- 主机名 / 内核 `hostname && uname -a`
- CPU 负载 `top -b -n1 | head -20`
- 当前登录用户 `who`

### 5.3 进程与日志

- **进程**：填关键字 → 查看进程 / 杀进程（`pkill -f`）。
- **日志**：填日志路径 + 行数 → `Tail` 末尾 N 行；填关键字 → `Grep` 查找。

等价命令示例：

```bash
ansible -i hosts web -m shell -a "ps -ef | grep -v grep | grep nginx"
ansible -i hosts web -m shell -a "tail -n 200 /var/log/messages"
ansible -i hosts web -m shell -a "grep -n -- ERROR /var/log/messages | tail -n 200"
```

### 5.4 文件操作

- **下载**：远端 → 本地，使用 `fetch` 模块。
- **删除**：远端文件或目录，使用 `file state=absent`。
- **建目录**：使用 `file state=directory`。
- **改权限**：使用 `file mode=...`。

### 5.5 软件包

- 输入包名后选择 `apt 安装/卸载`、`yum 安装/卸载` 或 **自动识别**（`package` 模块）。

等价命令：

```bash
ansible -i hosts web -m package -a "name=vim state=present"
```

## 6. 批量部署（高级，需要 playbook）

打开 `批量部署` 标签页。

### 5.1 执行 Playbook

步骤：

1. 在基础配置里选择 `工作目录`。
2. 选择 `Inventory`。
3. 填写 `主机组`，例如 `web`。
4. 在 Playbook 下拉框中选择要执行的 YAML 文件。
5. 检查下方 `命令预览`。
6. 点击 `执行选中 Playbook`。
7. 程序会打开外部终端执行任务。
8. 在日志区域查看同步输出和退出码。

常见 playbook：

```text
ansible.yaml
ansible-start.yaml
ansible-delete.yaml
ansible-redeploy.yaml
ansible-check-agent.yaml
```

对应命令类似：

```bash
ansible-playbook -i hosts ansible-redeploy.yaml
```

### 5.2 Ping 测试

点击 `Ping 测试` 会执行类似命令：

```bash
ansible -i hosts web -m ping
```

如果返回 `pong`，说明连通性正常。

## 7. 批量命令（手动 ad-hoc）

打开 `批量命令` 标签页。

### 6.1 使用 shell 模块

模块选择：

```text
shell
```

参数输入：

```bash
ps -ef | grep serveragent
```

点击 `批量执行命令`。

等价命令：

```bash
ansible -i hosts web -m shell -a "ps -ef | grep serveragent"
```

### 6.2 使用 command 模块

模块选择：

```text
command
```

参数输入：

```bash
hostname
```

等价命令：

```bash
ansible -i hosts web -m command -a "hostname"
```

### 6.3 常用命令示例

查看进程：

```bash
ps -ef | grep serveragent
```

查看目录：

```bash
ls -l /root/work/missionschedulingsoftware/
```

查看磁盘：

```bash
df -h
```

查看服务状态：

```bash
systemctl status sshd
```

## 8. 批量上传

打开 `批量上传` 标签页。

步骤：

1. 点击 `选择本地文件/目录`。
2. 选择要上传的程序文件或目录。
3. 填写远端路径。
4. 检查命令预览。
5. 点击 `批量上传`。

默认远端路径：

```text
/root/work/missionschedulingsoftware/
```

等价命令：

```bash
ansible -i hosts web -m copy -a "src=本地路径 dest=/root/work/missionschedulingsoftware/"
```

## 9. 日志和停止任务

### 9.1 查看日志

任务执行时会打开外部终端，同时 GUI 下方日志区域会同步显示：

- Ansible 标准输出
- Ansible 错误输出
- 任务退出码

外部终端脚本和同步文件默认位于系统临时目录，例如：

```text
/tmp/ansible-pilot-run.sh
/tmp/ansible-pilot-output.log
/tmp/ansible-pilot-status.txt
```

退出码一般含义：

- `0`：成功
- 非 `0`：失败或部分失败

### 9.2 停止任务

当前 Ansible 任务在外部终端中执行。如果任务执行时间过长或命令写错，请在外部终端中按 `Ctrl+C` 停止。

界面中的：

```text
停止当前任务
```

只用于停止 GUI 内部直接启动的进程；外部终端任务需要在终端窗口中停止。

## 10. 常见问题

### 10.1 提示找不到 ansible

原因：系统 PATH 中没有 `ansible`。

解决：请在当前 Linux 系统安装 Ansible，并确保 `ansible`、`ansible-playbook` 在 `PATH` 中。

如果外部终端中提示找不到命令，请检查终端环境：

```bash
echo $PATH
which ansible
which ansible-playbook
```

### 10.2 SSH 登录失败

常见原因：

- Inventory 用户名不对。
- SSH 密钥未配置。
- 远端服务器不可达。
- 防火墙或网络不通。

可以先在命令行测试：

```bash
ssh root@172.10.10.11
```

### 10.3 Ping 测试失败

可以检查：

- Inventory 路径是否选对。
- 主机组是否填写正确。
- 目标服务器 IP 是否正确。
- SSH 是否能登录。

### 10.4 Playbook 执行失败

请重点查看日志中的：

```text
FAILED
fatal
stderr
```

然后根据错误修改 playbook 或远端环境。

### 10.5 上传失败

检查：

- 本地路径是否存在。
- 远端目标目录是否存在。
- 远端用户是否有写权限。

### 10.6 提示 /dev/shm 或 multiprocessing 权限错误

如果日志中出现：

```text
ERROR! Unable to use multiprocessing, this is normally caused by lack of access to /dev/shm
```

说明 Ansible 或 Python multiprocessing 无法创建共享内存或 semaphore。

**最常见原因：`/dev/shm` 中堆积了 `sem.mp-*` 残留信号量**。每次 Ansible 异常退出或被关闭终端时，可能留下 `sem.mp-*`、`mp-*` 文件，累计到一定数量后新进程创建 `SemLock` 会被拒绝。

程序在每次执行 Ansible 前会自动：

1. 统计 `/dev/shm` 文件数和 `sem.mp-*`、`mp-*` 残留数量。
2. 删除当前用户名下的残留对象。
3. 重新尝试 Python multiprocessing 自检。
4. 任务结束后再次清理本次运行产生的残留。
5. 通过 `setsid` 让脚本脱离 GUI 父 session，避免命名空间继承问题。

如果仍然失败，请在外部终端中手动确认：

```bash
ls -ld /dev/shm
ls /dev/shm/sem.mp-* 2>/dev/null | wc -l
rm -f /dev/shm/sem.mp-* /dev/shm/mp-*
python3 -c "import multiprocessing as mp; q=mp.Queue(); q.put('ok'); print(q.get())"
```

正常情况下 Python 命令应输出：

```text
ok
```

如果手动清理后仍报错，可能是命名空间或 AppArmor 限制，需要从普通终端启动 AnsiblePilot，或重启系统。

## 11. 推荐使用流程

第一次使用建议按这个顺序：

1. 选择工作目录。
2. 点击 `刷新扫描`。
3. 选择 Inventory。
4. 主机组填写 `web`。
5. 点击 `Ping 测试`。
6. Ping 成功后再执行 playbook 或批量命令。
7. 每次执行前先看 `命令预览`。
8. 执行后看日志确认结果。

## 12. 示例操作

### 12.1 重新部署 serveragent

1. 工作目录选择 Ansible 根目录。
2. Inventory 选择 `hosts` 或对应目录下的 `my_hosts`。
3. 主机组填写 `web`。
4. Playbook 选择：

   ```text
   ansible-redeploy.yaml
   ```

5. 点击 `执行选中 Playbook`。

### 11.2 检查 serveragent 进程

1. 打开 `批量命令`。
2. 模块选择 `shell`。
3. 参数填写：

   ```bash
   ps -ef | grep serveragent
   ```

4. 点击 `批量执行命令`。

### 11.3 上传程序目录

1. 打开 `批量上传`。
2. 选择本地程序目录。
3. 远端路径填写：

   ```text
   /root/work/missionschedulingsoftware/
   ```

4. 点击 `批量上传`。
