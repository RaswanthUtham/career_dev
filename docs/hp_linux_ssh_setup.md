# HP Linux Builder: SSH First, Then Staged Tool Installation

> **2026-10-26 scope update:** Windows is the primary MCU build/debug host.
> Ubuntu Server installation is learner-reported; this SSH guide is optional,
> not a prerequisite. SSH and host/MCU tool setup remain useful if selected. Yocto,
> Linux-kernel cross-build packages and module exercises below are historical
> suggestions, not active installation requirements. Follow the
> [MCU setup guide](workspace_setup.md) and pin FreeRTOS/Zephyr dependencies.

Prepared 2026-10-23; simplified with inline command comments and syntax tables on 2026-10-25.
**Documentation-reviewed, not executed on the HP.**
The supplied setup proposal describes **16 GB RAM** and a **220 GB SSD**. Confirm
those values on the HP first (total SSD size is not the same as free disk space).
Use this guide alongside [workspace_setup.md](workspace_setup.md#optional-ubuntu-server-host).

Run commands one step at a time on the machine named above each block (**HP Ubuntu**
or **Windows PowerShell**), and stop if any command prints an error.

---

## 1. Review Of The Package Plan (What To Install Now vs. Later)

Instead of installing every compiler, Yocto package, and RTOS tool in one giant
command on Day 1, stage them in the order our roadmap actually uses them:

| Proposed step | Review decision | Why (in plain English) |
| --- | --- | --- |
| **1. Check OS, CPU, RAM & disk** | **Keep (run first)** | Confirms how much of the 220 GB SSD is actually free on your home/build partition (`ext4`), because Ubuntu's installer sometimes creates a smaller default root LV. |
| **2. `sudo apt update && sudo apt upgrade -y`** | **Split into 2 steps** | Run `sudo apt update` and `apt list --upgradable` first so you can see if a kernel/reboot upgrade is waiting before auto-installing with `-y`. |
| **3. Host C + Kernel + QEMU + Cross-Compilers** | **Install in stages** | Start with `build-essential` and `git` right after SSH works; add the Arm Linux cross-compiler (`gcc-arm-linux-gnueabihf`), kernel build libs, and `qemu-system-arm` for Week 2. |
| **4. `gcc-arm-linux-gnueabihf` vs. `gcc-arm-none-eabi`** | **Keep distinct** | `arm-linux-gnueabihf-gcc` compiles **32-bit Arm Linux code** (BeagleBone Black). `arm-none-eabi-gcc` compiles **bare-metal/RTOS MCU code** (STM32F446RE / F767ZI) with no OS. |
| **5. Yocto 5.0.14 (`scarthgap`) packages** | **Wait until Week 5 container/release choice** | Yocto 5.0.14 (`scarthgap`) officially tests up to **Ubuntu 24.04 LTS**, while **Yocto 6.0 (`wrynose`)** lists **Ubuntu 26.04 LTS**. If we use `scarthgap` on Ubuntu 26.04, we can install the Yocto packages inside an **Ubuntu 24.04 Podman container** on the HP so host GCC 15/14 never breaks older recipes. |
| **6. AppArmor user-namespace check** | **Inspect first, don't disable blindly** | Ubuntu 24.04+ and 26.04 set `kernel.apparmor_restrict_unprivileged_userns = 1`.Note that a Podman container shares the HP's Linux kernel, so we check the exact behavior in Week 5 rather than turning off security globally today. |
| **7. Zephyr & `west`** | **Install in Week 10 inside a `venv`** | Never run `sudo pip install`. Create a Python virtual environment (`python3 -m venv`) when we pin the Zephyr release and SDK in Week 10. |
| **8. Version check with `\| head -n1`** | **Use `--version` directly** | Piping `missing-cmd --version \| head -n1` can hide a nonzero exit status because `head` succeeds even if the compiler is missing. Run each `--version` directly. |

---

## 2. HP Console: Check System Info And Find Your LAN IP

Sit at the HP keyboard/screen for this first step and log in as your normal
Ubuntu user (not `root`). Both your Windows laptop and the HP must be on the
same home/office Wi-Fi or Ethernet LAN.

**Run on HP (Bash):**

```bash
# 1. Show exact Ubuntu version (e.g., Ubuntu 26.04 LTS)
lsb_release -a

# 2. Show Linux kernel version (-r) and CPU architecture (-m, e.g., x86_64)
uname -rm

# 3. Show your Ubuntu login username (you will need this for ssh user@ip)
whoami

# 4. Show total and available RAM in human-readable units (-h: GB/MB)
free -h

# 5. Show disk filesystem type (-T, e.g., ext4) and free space (-h: GB) for / and your home folder (~)
df -Th / ~

# 6. Show a brief (-br) table of IPv4 (-4) addresses on your network cards
ip -br -4 addr
```

### Command & Syntax Breakdown (Step 2)

| Command & flags | What each word/flag means | What to look for in the output |
| --- | --- | --- |
| `lsb_release -a` | **L**inux **S**tandard **B**ase release info; `-a` = show **all** fields | Confirms `Ubuntu 26.04 LTS` |
| `uname -rm` | Print system info: `-r` = kernel **release**, `-m` = **machine** CPU (`x86_64`) | Confirms 64-bit x86 kernel |
| `whoami` | Prints your current logged-in username | Write this down as `HP_USER` |
| `free -h` | Shows RAM & swap usage; `-h` = **human-readable** (`Gi` / `Mi`) | `total` around `15Gi–16Gi` |
| `df -Th / ~` | **D**isk **f**ree space: `-T` = print filesystem **type** (`ext4`), `-h` = **human-readable** (`G`), for root (`/`) and home (`~`) | Check the `Avail` column! If Ubuntu LVM only gave `/` 100 GB out of 220 GB, we can expand the logical volume before Yocto week |
| `ip -br -4 addr` | Network addresses: `-br` = **brief** one-line-per-interface format, `-4` = **IPv4** only | Look at your Wi-Fi (`wlo1`/`wlan0`) or Ethernet (`eno1`/`eth0`) line showing `UP` and grab the IP before the `/` (e.g., `192.168.1.50` from `192.168.1.50/24`). Ignore `lo` (`127.0.0.1`) |

---

## 3. HP Console: Install And Start The OpenSSH Server

SSH lets you open a terminal on the HP from your Windows laptop over your local
network.

**Run on HP (Bash):**

```bash
# 1. Refresh the list of available packages from Ubuntu repositories
sudo apt update

# 2. Install the OpenSSH server package (safe to run even if already installed)
sudo apt install -y openssh-server

# 3. Start the SSH service right now (--now) and enable it on every boot (enable)
sudo systemctl enable --now ssh

# 4. Check that the SSH service is "active (running)" without pausing in a pager
systemctl status ssh --no-pager

# 5. Verify that port 22 is listening (-t = TCP, -l = listening, -n = numeric ports)
sudo ss -tlnp | grep :22

# 6. Show this HP server's unique Ed25519 fingerprint (-l = list fingerprint, -f = file)
ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub
```

### Command & Syntax Breakdown (Step 3)

| Command & flags | What each word/flag means | Why we run it |
| --- | --- | --- |
| `sudo apt update` | `sudo` = run as admin; refreshes package index lists | Ensures `apt` knows the latest package URLs |
| `sudo apt install -y openssh-server` | Installs the SSH daemon (`sshd`); `-y` = answer "yes" to install prompt | Enables incoming SSH connections |
| `sudo systemctl enable --now ssh` | Controls systemd services: `enable` = start at boot; `--now` = also start immediately | Starts SSH now and after any reboot |
| `systemctl status ssh --no-pager` | Shows service health; `--no-pager` prints directly to screen without opening `less` | Look for `Active: active (running)` (or `ssh.socket` active on socket-activated Ubuntu) |
| `sudo ss -tlnp \| grep :22` | **S**ocket **s**tatistics: `-t` (**T**CP), `-l` (**l**istening), `-n` (**n**umeric port `22`), `-p` (**p**rocess name) | Confirms port `:22` is open and listening |
| `ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub` | `-l` = **l**ist fingerprint of `-f` (**f**ile) | Prints a `SHA256:...` string. When Windows connects in Step 4, it will show this exact same `SHA256:...` string so you know you are connecting to your HP |

### Optional Firewall Check (Only If Ubuntu Firewall `ufw` Is Active)

```bash
# Check if Ubuntu's Uncomplicated Firewall (ufw) is active or inactive
sudo ufw status
```
* If it says **`Status: inactive`** (the default on a fresh Ubuntu Server install), you don't need to do anything—proceed to Step 4.
* If it says **`Status: active`**, allow SSH from your Windows laptop's LAN IP (replace `192.168.1.10` with your Windows IP from `ipconfig`):
  ```bash
  # Allow TCP port 22 (SSH) from your Windows laptop's LAN IP
  sudo ufw allow from 192.168.1.10 to any port 22 proto tcp
  ```

---

## 4. Windows Laptop: Test First SSH Login (With Password)

Open **Windows PowerShell** on your Windows laptop. Replace `hp_user` with the
username from `whoami` and `192.168.1.50` with the IP from `ip -br -4 addr`:

**Run on Windows (PowerShell):**

```powershell
# 1. Check that Windows OpenSSH client is installed
ssh -V

# 2. Test that Windows can reach TCP port 22 on the HP over Wi-Fi/LAN
Test-NetConnection -ComputerName 192.168.1.50 -Port 22

# 3. Connect to the HP over SSH (replace hp_user and 192.168.1.50)
ssh hp_user@192.168.1.50
```

### What Happens During `ssh hp_user@192.168.1.50`:
1. Windows prints:
   `ED25519 key fingerprint is SHA256:... Are you sure you want to continue connecting (yes/no/[fingerprint])?`
2. Check that the `SHA256:...` string matches Step 3 on the HP screen, then type `yes` and press Enter.
3. Type your Ubuntu password and press Enter (nothing appears on screen while typing passwords—that is normal).
4. Once logged in, run `hostname` and `whoami` to see your HP prompt, then type `exit` to return to Windows PowerShell for Step 5.

---

## 5. Windows Laptop: Set Up Passwordless/Key-Based SSH Login

Typing your Ubuntu account password on every connection gets old fast. Here we
create a dedicated **Ed25519 SSH key pair** on Windows and copy only the
**public key (`.pub`)** to the HP.

**Run on Windows (PowerShell):**

```powershell
# 1. Create the ~/.ssh folder on Windows if it does not exist yet
mkdir -Force "$HOME\.ssh"

# 2. Generate a new Ed25519 key pair named hp-x360_ed25519
#    -t ed25519 : modern, fast elliptic-curve key type
#    -f <path>  : output file path (creates private key + .pub public key)
#    -C <label> : human-readable comment at the end of the public key
#    (If asked "Overwrite (y/n)?", type n so you never overwrite an existing key!)
ssh-keygen -t ed25519 -f "$HOME\.ssh\hp-x360_ed25519" -C "hp-x360"

# 3. Copy the public key (.pub) to the HP's ~/.ssh/authorized_keys file
#    (Replace hp_user and 192.168.1.50 with your HP username and IP)
Get-Content "$HOME\.ssh\hp-x360_ed25519.pub" | ssh hp_user@192.168.1.50 "mkdir -p ~/.ssh && chmod 700 ~/.ssh && tr -d '\r' >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys"

# 4. Test logging in with your new private key (-i = identity/key file)
ssh -i "$HOME\.ssh\hp-x360_ed25519" -o PasswordAuthentication=no hp_user@192.168.1.50
```

### Command & Syntax Breakdown (Step 5)

| Command part | Plain meaning |
| --- | --- |
| `ssh-keygen -t ed25519 -f ...` | Creates two files in `C:\Users\<you>\.ssh\`:<br>1. `hp-x360_ed25519` (**Private key** — stays secret on Windows forever)<br>2. `hp-x360_ed25519.pub` (**Public lock** — safe to copy to the HP) |
| `Get-Content "...pub" \| ssh ...` | Reads the public key line on Windows and pipes (`\|`) it over SSH into the command on the HP |
| `mkdir -p ~/.ssh` | Creates `~/.ssh` on the HP if missing (`-p` = no error if it already exists) |
| `chmod 700 ~/.ssh` | Gives only your user (`7` = read+write+enter) access to `~/.ssh`, and `00` to everyone else (SSH refuses to use keys if permissions are too open!) |
| `tr -d '\r' >> ~/.ssh/authorized_keys` | Strips any Windows carriage-return (`\r`) character and **appends (`>>`)** the key to `authorized_keys` without erasing existing keys |
| `chmod 600 ~/.ssh/authorized_keys` | Sets file permissions to owner read+write (`600`), required by `sshd` |
| `ssh -i <key> -o PasswordAuthentication=no ...` | `-i` selects your private key file; `-o PasswordAuthentication=no` proves key login works without falling back to your Ubuntu password |

---

## 6. Windows Laptop: Save a Short Alias (`ssh hp`) & Connect VS Code

So you never have to type the IP address or `-i` key path again, add a 5-line
entry to `C:\Users\<you>\.ssh\config` on Windows:

**Run on Windows (PowerShell) — or open `$HOME\.ssh\config` in VS Code:**

```text
Host hp
    HostName 192.168.1.50
    User hp_user
    IdentityFile ~/.ssh/hp-x360_ed25519
    IdentitiesOnly yes
```
*(Replace `192.168.1.50` and `hp_user` with your HP's IP and username.)*

Now, from any Windows PowerShell window or VS Code **Remote - SSH**:
* In PowerShell, just type:
  ```powershell
  ssh hp
  ```
* In VS Code, press `Ctrl+Shift+P` -> **Remote-SSH: Connect to Host...** -> select **`hp`**. Your VS Code window stays on Windows while your files, terminal, compiler, and QEMU run directly on the HP!

---

## 7. On HP (Over SSH): Staged Package Installation

Once you are logged into the HP via `ssh hp`, install packages in clear stages.

### Stage A: Check Upgrades & Install Base C / Git Tools (Week 1 — Run Now)

```bash
# 1. See which packages have upgrades waiting (review before running `sudo apt upgrade`)
apt list --upgradable

# 2. Install the native C/C++ compiler (gcc, g++, make, libc6-dev) and git
sudo apt install -y build-essential git

# 3. Verify each tool directly (do NOT pipe into `head`, which hides errors)
gcc --version
make --version
git --version
```

### Stage B: Linux Kernel Build, 32-Bit Arm Cross-Compiler & QEMU (Week 2)

Install these when we start Week 2 (Linux QEMU kernel & BeagleBone Black 32-bit
Arm driver builds):

```bash
# 1. Install Linux kernel build libraries (bc, bison, flex, libssl-dev, libelf-dev,
#    libncurses-dev, dwarves/pahole, device-tree-compiler, u-boot-tools, cpio, kmod)
sudo apt install -y \
  bc bison flex libssl-dev libelf-dev libncurses-dev dwarves \
  cpio kmod fdisk u-boot-tools device-tree-compiler pkg-config

# 2. Install 32-bit Arm Linux cross-compiler (for BeagleBone Black), multi-arch GDB, and Arm QEMU
sudo apt install -y \
  gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf binutils-arm-linux-gnueabihf \
  gdb-multiarch qemu-system-arm qemu-utils

# 3. Verify each installed tool directly
arm-linux-gnueabihf-gcc --version
qemu-system-arm --version
gdb-multiarch --version
```

### Stage C: Yocto Build Host Setup (Week 5 — Hold Until Release/Container Choice)

Before running BitBake in Week 5, check these two commands on the HP:

```bash
# 1. Check if en_US.UTF-8 locale is already present (Yocto requires UTF-8)
locale -a | grep -i en_us.utf8

# 2. Check Ubuntu's AppArmor unprivileged user-namespace sysctl setting
sysctl kernel.apparmor_restrict_unprivileged_userns
```

* If `en_US.utf8` is missing from `locale -a`, enable it with:
  ```bash
  sudo locale-gen en_US.UTF-8
  sudo update-locale LANG=en_US.UTF-8
  ```
* **When we reach Week 5**, we will install the official Yocto host packages either inside a **pinned Ubuntu 24.04 Podman container** (if building Yocto 5.0 `scarthgap`) or natively (if using a release tested on Ubuntu 26.04):
  ```bash
  sudo apt install -y \
    build-essential chrpath cpio debianutils diffstat file gawk gcc git \
    iputils-ping libacl1 liblz4-tool locales python3 python3-git \
    python3-jinja2 python3-pexpect python3-pip python3-subunit \
    socat texinfo unzip wget xz-utils zstd
  ```

### Stage D: STM32 Bare-Metal & Zephyr RTOS Tools (Weeks 8 & 10 — Hold Until Needed)

When we reach Week 8 (STM32F446RE acquisition) and Week 10 (Zephyr RTOS), we
will install the MCU tools and create a clean Python virtual environment for `west`:

```bash
# 1. Bare-metal Arm Cortex-M compiler (no OS), CMake/Ninja, OpenOCD, and picocom serial monitor
sudo apt install -y \
  gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi \
  cmake ninja-build gperf ccache dfu-util python3-dev python3-venv \
  python3-setuptools python3-wheel libmagic1 openocd picocom bmap-tools

# 2. Verify bare-metal compiler and OpenOCD
arm-none-eabi-gcc --version
openocd --version
```

---

## 8. Evidence To Record In Chat After Step 2–7 (Stage A)

Once you finish **Step 2 through Step 7 (Stage A)** on the HP, paste the output
of these commands in chat so we can mark the HP build-host gate verified in
[training_tracker.md](../plans/training_tracker.md):

```bash
lsb_release -ds && uname -rm && free -h && df -Th / ~ && gcc --version
```

## References

- [Ubuntu OpenSSH server](https://ubuntu.com/server/docs/how-to/security/openssh-server/)
- [Microsoft OpenSSH key management](https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh_keymanagement)
- [VS Code Remote-SSH](https://code.visualstudio.com/docs/remote/ssh)
- [Yocto 5.0.14 host requirements](https://docs.yoctoproject.org/5.0.14/ref-manual/system-requirements.html)
- [Zephyr getting started](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
