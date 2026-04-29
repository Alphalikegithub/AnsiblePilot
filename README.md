# AnsiblePilot 使用说明

`AnsiblePilot` 是一个基于 Qt/C++ 的 Ansible 可视化批量运维工具，用来把常用的 Ansible 操作做成图形界面，方便不会频繁敲命令的人快速执行批量上传、批量部署、批量命令和连通性测试。

## 1. 功能概览

当前程序支持以下功能：

- **自动扫描 Ansible 文件**：扫描工作目录下的 `*.yaml`、`*.yml`、`hosts`、`my_hosts`、`*.ini`。
- **批量部署**：选择已有 playbook，例如 `ansible.yaml`、`ansible-start.yaml`、`ansible-redeploy.yaml` 后执行。
- **Ping 测试**：调用 `ansible -m ping` 检查目标主机连通性。
- **批量执行命令**：通过 `shell`、`command`、`service`、`systemd` 等模块批量执行命令。
- **批量上传**：通过 Ansible `copy` 模块把本地文件或目录上传到远端服务器。
- **执行日志展示**：实时显示 Ansible 标准输出和错误输出。
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

### 3.2 准备 SSH 访问

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

## 5. 批量部署

打开 `批量部署` 标签页。

### 5.1 执行 Playbook

步骤：

1. 在基础配置里选择 `工作目录`。
2. 选择 `Inventory`。
3. 填写 `主机组`，例如 `web`。
4. 在 Playbook 下拉框中选择要执行的 YAML 文件。
5. 检查下方 `命令预览`。
6. 点击 `执行选中 Playbook`。
7. 在日志区域查看执行结果。

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

## 6. 批量命令

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

## 7. 批量上传

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

## 8. 日志和停止任务

### 8.1 查看日志

下方日志区域会实时显示：

- Ansible 标准输出
- Ansible 错误输出
- 任务退出码

退出码一般含义：

- `0`：成功
- 非 `0`：失败或部分失败

### 8.2 停止任务

如果任务执行时间过长或命令写错，可以点击：

```text
停止当前任务
```

程序会终止当前 Ansible 进程。

## 9. 常见问题

### 9.1 提示找不到 ansible

原因：系统 PATH 中没有 `ansible`。

解决：请在当前 Linux 系统安装 Ansible，并确保 `ansible`、`ansible-playbook` 在 `PATH` 中。

### 9.2 SSH 登录失败

常见原因：

- Inventory 用户名不对。
- SSH 密钥未配置。
- 远端服务器不可达。
- 防火墙或网络不通。

可以先在命令行测试：

```bash
ssh root@172.10.10.11
```

### 9.3 Ping 测试失败

可以检查：

- Inventory 路径是否选对。
- 主机组是否填写正确。
- 目标服务器 IP 是否正确。
- SSH 是否能登录。

### 9.4 Playbook 执行失败

请重点查看日志中的：

```text
FAILED
fatal
stderr
```

然后根据错误修改 playbook 或远端环境。

### 9.5 上传失败

检查：

- 本地路径是否存在。
- 远端目标目录是否存在。
- 远端用户是否有写权限。

## 10. 推荐使用流程

第一次使用建议按这个顺序：

1. 选择工作目录。
2. 点击 `刷新扫描`。
3. 选择 Inventory。
4. 主机组填写 `web`。
5. 点击 `Ping 测试`。
6. Ping 成功后再执行 playbook 或批量命令。
7. 每次执行前先看 `命令预览`。
8. 执行后看日志确认结果。

## 11. 示例操作

### 11.1 重新部署 serveragent

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
