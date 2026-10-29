# Leica M10 工具链 · 四项功能做到哪一步

[English](README.en.md) · 普通 **M10** · 独立项目，与 Leica 无隶属关系

四项功能已在**我自己的机器上**跑通：**XPan 画幅、ASC 滤镜、胶片滤镜、M10的屏幕触控**。
它们现在还不是公共方案，因为每项都绑着那一台机器的标定、那一份原厂资源、
和固件版本。

这个仓库存在的理由，就是诚邀大家把它们从"在我个人机器上成立"推到"在任何一台 M10 上成立"。
**第 1 节写已经跑通了什么，第 2 节写缺什么，第 3 节写为什么只在一台机器上成立。**
每一项缺口都可以独立认领。

## 从这里开始

| 文档 | 内容 |
|---|---|
| **[docs/status.md](docs/status.md)**（[EN](docs/status.en.md)） | **现状与边界**：四项功能各自跑通到哪、差什么、怎么认领 |
| **[docs/quickstart.md](docs/quickstart.md)**（[EN](docs/quickstart.en.md)） | **五分钟上手**：全程离线、不需要相机 |
| **[docs/contributing-ideas.md](docs/contributing-ideas.md)** | **不需要相机的具体任务**，每条带入口文件与验收标准 |
| [docs/architecture.md](docs/architecture.md) | **各部分怎么拼**：源码布局、host 与 native 的边界、哪些还没有资格 |

## 这个仓库不做什么

**没有固件写入器。** 没有刷机路径、没有设备协议发送器、没有可以生成的 `.FW`。
仓库里全部是对你自己已拥有的文件做分析、校验、离线重算——它不会碰你的相机。

固件写入、身份改写、包构造这类能力留在私有树里，等公共讨论有结论再说。
这些固件都是官网可以Fetch的公开固件。搭配我的框架就可以进行研究了，Github有限制，我无法直接上传。
这**不影响**你要做的事：上面列的每一项缺口，都不需要写入能力就能推进。

## 关于 `package-*` / `models` / `identify` 这组命令

这组 CLI **已经注册但不可用**：它们依赖的 `leica_source/package_edit.py`、
`package_profile.py`、`models.py` **不在本树里**。调用时会明确拒绝并提示替代路径，
而不是抛 ImportError。只读的包检查请用 `codec/`：

```sh
node codec/package_builder.mjs inspect YOUR.FW
node codec/package_builder.mjs diff ORIGINAL.FW REBUILT.bin
```

需要包编辑/组包能力时，请在 issue 里说明用途，而不是假设它已经把代码一起公开了。

## 先读这两份

| 文档 | 内容 |
|---|---|
| **[docs/quickstart.md](docs/quickstart.md)**（[EN](docs/quickstart.en.md)） | **五分钟上手**：全程离线、不需要相机，确认工具链 → 求值一款风格 → 跑主机参考实现 |
| **[docs/contributing-ideas.md](docs/contributing-ideas.md)** | **离线能做的具体工作**：复验一条文档结论、加固一个解析器、给符号提取器加测试 |
| [docs/native-development.md](docs/native-development.md) | **native 开发**：便携组合 / 提取 / 校验 / 反向流程，以及哪些仍未取得资格 |
| [docs/verification.md](docs/verification.md) | **验证记录**：跑过什么、用了哪些本地输入，以及"主机测试通过"**不**等于什么 |

## 先验证，再相信

这个仓库不重分发任何 Leica 固件。所有结论都是"关于你自己那份固件里某些字节"的陈述，
所以每一步都可以自己复算：

```sh
python3 codec/independent_verify.py /path/to/M10-4.26.36.10-Customer.FW
```

它不依赖 JavaScript codec，独立复验整包：外层 magic 与格式、COMP 解码、TOC 检查，
并对照固定的整包 SHA-256 与 277 项 payload 身份。输出 JSON 含 `accepted`、
`payload_sha256`、`components`、`blocks`；不匹配会指出具体哪一项 pin 失败。

它需要你自己提供一份固件包。在完全没有固件的情况下，仍可用合成样本检验 codec：
`node --test codec/test_builder.mjs`（24 个用例，含反例）。详见
[docs/quickstart.md](docs/quickstart.md)。

**不需要相机，不需要我们的私有工作文件，也不写任何东西**（只解包到临时目录）。
一次完整解码大约 2–3 分钟，因为要展开 127 MB 的包。
修改与刷写则是另一回事：**这台机器只有一次刷写机会，而且没有独立恢复源**，
风险由操作者自行承担——详见 modification-path 的第 6 节。

## 便携 native 开发流程

完整源码 checkout 现在提供普通 M10 **4.26.36.10** 的实际 native IMG 组合：
ASC 两个原布尔控制、独立 PTP/A431 软件报告值，以及 fine 或 legacy WB。
输出始终是 **NOT-INSTALLABLE IMG** 与完整 277 项 metadata 计划；没有定制 FW、
压缩改装包、encoder admission、安装器、boot/recovery 或硬件验证。

```sh
python3 -B -m leica_source extract-native --official-fw /path/to/M10-4.26.36.10-Customer.FW --asc-fw /path/to/M10-P-ASC-4.26.36.10-Customer.FW --output-dir /path/to/private/extracted
python3 -B -m leica_source build-native --inputs /path/to/private/extracted --config examples/native-synthetic-combined.json --output-dir /path/to/private/combined
python3 -B -m leica_source verify-native --inputs /path/to/private/extracted --input-dir /path/to/private/combined
python3 -B -m leica_source reverse-native --inputs /path/to/private/extracted --input-dir /path/to/private/combined --output-dir /path/to/private/reversed
```

这些路径是使用者自行选择的本地位置；所有输出必须是源码 checkout 外的全新目录，
其父目录须已存在。示例 `0000123` 与 `(257,256,255)/256` 都是合成测试选择，
不是使用者机号或校准建议。ORIGINAL 用 `examples/native-original.json`；
ASC 关闭时 `extract-native` 可省略 `--asc-fw`，不会提取或输出 hybrid。
仅需 Python 标准库，不使用 ARM 工具链或本机研究目录。

native 配置采用独立的 `m10-native-composition/v2` schema。
旧 `selection.json` 的 22 款 HOST 样式/触摸/框线不会自动升级为 native；
旧 native v1 WB 保留五档语义。详情、输入 pins、窗口/资源边界和限制见
[portable native development](docs/native-development.md)。

## 整包编辑（`package-*`）

> **本节描述的能力不在本树里。** `leica_source/package_codec.py`、
> `package_edit.py`、`package_profile.py` 均未随本树发布，所以下面这组命令
> 目前只会明确拒绝（见上文说明）。容器只读检查请用 `codec/package_builder.mjs`
> 的 `inspect` / `diff`。保留本节是因为它描述了容器格式与验收标准，
> 供认领相关工作时参考。

直接操作外层更新容器的 277 条 TOC 记录、组件体，以及包裹它们的 COMP 流。
解码器与编码器都在 `leica_source/package_codec.py`，只用 Python 标准库，不需要 node。

接受标准是**字节级往返**：对官方 M10 4.26.36.10 包，`encode(decode(x)) == x` 成立；
且编码结果与独立 JS 实现产出的 SHA256 完全一致。

```sh
python3 -B -m leica_source package-info    M10-4.26.36.10-Customer.FW   # 秒级，只看头
python3 -B -m leica_source package-list    M10-4.26.36.10-Customer.FW   # 全组件表 + 哈希，约 80s
python3 -B -m leica_source package-extract M10-4.26.36.10-Customer.FW "/system/system.bin" --output system.bin
python3 -B -m leica_source package-diff    A.FW B.FW
python3 -B -m leica_source package-edit    --source M10-4.26.36.10-Customer.FW --plan plan.json --output-dir out
```

计划文件 `m10-package-edit/v1` 支持任意顺序的 `add` / `replace` / `remove`：

```json
{"schema":"m10-package-edit/v1","operations":[
  {"op":"add","path":"data/calib/JDI_tp_configuration.bin","file":"tp.bin",
   "template_fw":"M10-P-ASC-4.26.36.10-Customer.FW",
   "template_path":"data/calib/JDI_tp_configuration.bin"}
]}
```

`add` 的 TOC 记录取自指定的模板组件，改写数据 MD5、相对偏移、长度，并把记录的
`[32:36]` 型号字段改写为目标包的型号；名称与路径可用 `"name"` / `"path"` 覆盖。
组件选择支持 `#<index>`，因为组件路径并不唯一（镜头表重复多次）。

### 刻意不做的事

- **不改版本字段**
- 记录的型号字段只跟随目标包，不能任意指定
- 输出文件名不允许以 `.FW` 结尾
- 每次构建后强制重新解码并逐组件比对；未保留的组件写进 `verification.components_broken`

原因不是姿态：包头版本与实际内容一旦不一致，就无法再从机器上判断装的是什么。
而且**这道门本来就不在包里**——相机运行时做 `cmp new, actual` + `bhi`，
要求新版本严格大于已装版本（代码在 `system.bin` 偏移 `0x190F1C–0x190F48`）。
改包内容绕不过它。

### 验证锚点

| 操作 | 结果 |
|---|---|
| 官方包往返 | 字节相同，SHA256 不变 |
| M10 4.26 加入 M10-P 的 TP 配置 | `1476bf2c400697bfd4efb426aeb2a25fbbb600c78b2a5899cf0f92b4c3c7643f`（Python 与 JS 两套实现一致）|

`tests/test_package_codec.py` 覆盖头部/TOC 守卫与真实包元数据；
完整往返需要 `M10_SLOW_TESTS=1`（约两分钟）。

## 型号与容器识别（`models` / `identify`）

> **本节描述的能力不在本树里。** `leica_source/models.py` 未随本树发布，
> 所以 `models` / `identify` 目前只会明确拒绝。下面的表格是识别依据的记录，
> 供认领相关工作或自行实现时参考。

三层信息是分开的，不能混为一谈：

| 层 | 位置 | 取值 |
|---|---|---|
| **容器版本** | 头 4 字节 | `3e af 1c 1e` = `comp/v1`（M10 代）；`3e af 2c 2e` = `comp/v2`（M11 代）；`aa af bb ff` = Q 系 `.lfu` |
| **平台家族** | `type` @8 | M10 代全部 `0x07e1ea5e` |
| **型号 / 变体** | `hardware` @48 | 见下表 |

| hardware | 型号 | 依据包 |
|---:|---|---|
| 321 | M10 | 3.22.23.38 / 4.26.36.10 |
| 322 | M10-R | 40.26.35.51 |
| 323 | M10-P | 4.22.23.34 / 5.26.35.51 |
| 325 | M10-P (ASC) | 4.26.36.10 |
| 326 | M10-M | 5.26.35.51 |

`comp/v1` 与 `comp/v2` 只差第 2、3 字节（`1c 1e` vs `2c 2e`），**不是完全不同的魔数**——
这是同一个容器的两个修订版。所以 M11 不是"格式坏了"，是"还没实现"。

**M10-P 出现两个 hardware 值**（323 与 325）。所以型号名只是对这个包的标注，
不是对机身的承诺。

```sh
python3 -B -m leica_source models              # 注册表
python3 -B -m leica_source identify A.FW B.FW  # 逐包识别
```

目前只有 `comp/v1` 可编辑。M11 的 `comp/v2` 与 Q 系 `.lfu` 会被**识别并明确拒绝**，
不会误当成可编辑包：

```
{"error": "container comp/v2 is recognised but not implemented; only comp/v1 is editable"}
```

### 跨型号组合的边界

`package-matrix` / `package-build` 要求 base 与 donor **同属一个平台家族**，不同则不解码直接拒绝。
同一家族内跨型号是允许的，但会写进 `compatibility.notes`：

- 两侧各是什么型号、hardware 各是多少
- 版本不同时提示"构建保留 base 的版本"
- 明确声明组件体**未在目标机身上验证**

## 设置式编辑（`package-matrix` / `package-interactive` / `package-build`）

不想写计划文件时用这一层：以**一台机器的包为基准**，逐项选择从 **M10-P 包**取哪一块。
基准包决定版本号、型号字段和所有没被点名的组件；donor 包只提供组件体。

```sh
python3 -B -m leica_source package-matrix --base M10-4.26.36.10-Customer.FW \
                                          --donor M10-P-ASC-4.26.36.10-Customer.FW
python3 -B -m leica_source package-interactive --base ... --donor ... --output my-profile.json
python3 -B -m leica_source package-build --base ... --donor ... \
                                          --profile my-profile.json --output-dir out
```

`package-interactive` 只列出**两侧不一样**的项，逐条问：回车保留基准／`p` 取 M10-P／`d` 删除。
它写出 `my-profile.json`，你可以改完再 build——中间产物是文本，方便复核。

profile（`m10-package-profile/v1`）：

```json
{"schema":"m10-package-profile/v1","components":[
  {"path":"data/calib/JDI_tp_configuration.bin","from":"m10p"},
  {"path":"data/calib/B2Y.bin","from":"m10"},
  {"path":"data/lens/shb/Willi_24MP_49x33_test.shb","from":"drop"}
]}
```

### 组件键不是路径

同一路径在包里可能有多份：M10 包把绝大多数镜头表放了 **3 份**，内容逐字节相同。
所以键按路径归并——**一份设置作用于该路径的所有副本**。

只有当一个路径的多份内容真的不同时，键才带序号。M10 包里只有两个键属于这种情况：
`CTRL-System#0` / `CTRL-System#1` 和 `CTRL-Top System#0` / `CTRL-Top System#1`。
M10-P 各只有一份，因此显示为 `CTRL-System`。

这直接影响结果：早期版本按 `path#序号` 做键，会让 M10 的 3 份副本和 M10-P 的 1 份错配，
把 83 个组件误判成"只在一边存在"，并且 `build` 会静默丢组件。

### 现状矩阵（M10 4.26 基准 / M10-P ASC 4.26 donor）

| 关系 | 数量 |
|---|---|
| 两侧完全相同 | 24 |
| 两侧都有但不同 | 82 |
| 只在基准（M10）有 | 6 |
| 只在 donor（M10-P）有 | 4 |

只在 M10-P 有的四项：`data/calib/JDI_tp_configuration.bin`、`CTRL-System`、
`CTRL-Top System`、`data/lens/shb/M-1,4_35_Linz_VIGF_35mm.shb`。

### 一条必须显式接受的高危设置

取 donor 的 `/system/system.bin`（整块系统镜像跨型号替换）需要 profile 里写
`"accept_cross_model_system": true`。它不能靠批量"全选"触发，因为这是这里能造成的
最严重的一处改动。

### 交叉验证

| 路径 | 结果 |
|---|---|
| Node 手工构建 | `1476bf2c…` |
| `package-edit`（加 TP 配置） | `1476bf2c…` |
| `package-build`（同一改动的 profile） | `1476bf2c…` |

三条互不共享代码路径产出同一份字节，且 `verification.passed = true`。
`package-build` 全流程约 4.5 分钟（两次解码 + 编码 + 回落复验）。

## 启动与下载

需要 Python 3.10+。本地界面和源码导出只需标准库，无 pip/npm 安装。

```sh
python3 launch.py
# 浏览器打开 http://127.0.0.1:8780
```

Windows 可用 `py launch.py`。服务固定绑定回环地址；不连接相机或远程服务。
命令行同样可用（Windows 将 `python3` 换为 `py`）：

```sh
# 完整源码 checkout 首次运行下行；下载 ZIP 已含冻结 selection.json，直接使用它
python3 -m leica_source default-config --output selection.json
python3 -m leica_source validate selection.json
python3 -m leica_source build-source --config selection.json --output development-source.zip
```

导出包含所选源码模块及依赖、冻结配置、主机调用层、许可和逐文件 SHA256。
下载包的界面与 CLI 默认读取冻结的 `selection.json`；未包含模块的控件不可用，
验证和再次导出会明确拒绝选择缺失模块。只打包依赖已包含的 `examples/` 配置。
完整 checkout 未有冻结配置时，界面默认可打包所有已包含样式。
配置严格拒绝未知键、重复键、错误类型、非普通 M10 基线和伪造资格字段。
不会把本机原厂输入路径写进 ZIP。

## HOST 参考已经能运行什么

主机执行需要 C11 编译器（macOS clang、Linux gcc/clang，Windows MinGW-w64），
环境变量 `CC` 可指定编译器。生成的本机动态库只执行项目 C 源，不执行 ARM。

```sh
python3 -m leica_source compile-sources --build-dir build/objects
python3 -m leica_source run-reference --config selection.json --build-dir build/host --rgb 120,80,40
```

| 选择 | 实际调用与结果 | 相机上仍缺什么 |
|---|---|---|
| 普通触摸 | `tr_completed_tail` 实际分派、清理、UNKNOWN 资源保留；真实 caller/result pool 源码随附 | 真实 producer、7 参数 ABI、pool 生命周期/同步/地址、加载与恢复 |
| 软件报告序列号 | 实际 `ri_json_locked/freeze/bind/run`：ORIGINAL 验证后，将冻结报告值交给 A431/private 序列化；保留前导零，旧照片不随之后编辑改变 | 普通 M10 factory 调用方、原生 owner、13 imports/serializer ABI 仍未绑定；P 照片路径未实现；不改原始身份/校准/历史 |
| ASC 两款 look | `m10_b2y_parse` → `m10_asc_preflight` 选择真实 CC1/RGBY 资源、完整/消费长度；closed apply 拒绝 | acquire/idle/cache/epoch/hardware/publish/restore 导入；没有像素 renderer |
| 19 款胶片配方 | 800T 真实公式；18 GRKit 配方（14 彩色 / 4 黑白）通过已接受通用引擎实际求值，保留可选 hue/post、knots/selective 分支 | GRKit 本地精确 gamma；M10 输入域/原生 bank/性能/selector/loader |
| ASC 风格 / 取景框组合 | `sfc_freeze/apply/restore` 直接调用 ASC、LVF、普通 M10 u16 ID 后端；真实 PARTIAL 与各域恢复责任 | 真 GUI owner/锁/显示域、同帧 stock-hide、redraw/退出恢复及 ASC 原生导入 |
| WB/DNG 校准 | 原 C shot begin/future reset/finish，用冻结增益调整 24B AsShotNeutral | 同次曝光异步回调和元数据 owner；不是完整 DNG 文件写入器 |
| JPEG crop / 低噪快门 | 配置记录，主机执行明确拒绝 | 尚无可执行实现；取景框不裁切 JPEG/DNG，快门硬件未证明 |

主机回调在 `host/` 中明确标为 HOST，它们不替代生产 owner。新接受的 identity
C helper/header 也保留；原厂指令镜像因版权边界不分发，缺失符号不以测试替身补成生产代码。
所选七位序列号会实际进入新 C 配置、original+reported 捕获快照及报告值序列化。
原始记录/raw8/S 始终独立保留；当前软件报告可更新，旧捕获仍使用原冻结报告值。
主机示例默认 ORIGINAL 为声明的合成值 `1000001`，后续报告为 `0000002`，可明确指定：

```sh
python3 -m leica_source run-reference --config selection.json --original-serial 0001234 --next-reported-serial 0000009
```

输出同时列出 A431/private 报告值、原始值、后续当前值、clear/default 后值及旧捕获值。

> **注意：本仓库不含 identity 实现。** `host/identity/` 与 `modules/identity/` 都不在
> 这份代码树里，所以上面这条命令**不会产生 identity 输出**——它会静默返回
> `modules: [film, touch, wb, controller, framing]`，没有 `identity` 键。上面描述的
> 行为来自私有实现，这里保留的是调用侧约定，不是可运行的功能。

`host/identity/` 的 native-segment double 与 private-ready 只用于主机边界，不是原生 builder；
生产 `mb_thumb_builder` 继续 unresolved。没有持久化、全部身份覆盖或无痕承诺。

同一个下载包可以包含全部 **22 种样式**（Normal + 2 ASC + 800T + 18 GRKit）。
原有七个配置 ID 继续兼容，其余配方沿用 accepted effect ID。
`styles.enabled` 决定打包能力集合，
`styles.default` 是唯一当前样式，必须属于 enabled；ASC 与胶片模型输入域不同，
每个 shot 冻结这个选择并只路由一条管线，不把 ASC 与胶片叠加。
`run-reference` 先验证完整配置/必需数据，再调用所选实现；它不宣称产生相机 JPEG。
STYLE 与 GUIDES 是独立域：原生胶片分支在 stage 0 拒绝后，主机仍可单独调用 GUIDES；
ASC 成功而框线失败时报告 PARTIAL，恢复逐域执行。`host/controller/` 只提供显式主机
普通内存 / 资源回调，生产 imports 仍 closed；没有使用恢复出的固件记录作为公开 fixture。

## 本地原厂输入

从 [Leica 原站下载 M10 4.26.36.10](https://leica-camera.com/sites/default/files/M10-4.26.36.10-Customer.FW)。项目不提供固件镜像。

```text
127615696 bytes
SHA256 d6ef5b77b70451d2b6a31fc4a090bf8f664c1e07a8bebade657aff7da692423c
```

```sh
python3 -m leica_source verify-official /path/to/M10-4.26.36.10-Customer.FW
```

独立的参考重建需要 Node.js 20+。它复用既有真实 COMP 编码器，重建全部 277 TOC
记录和 9 COMP 块，**不加入所选功能**。命令输出 analysis.bin 和核对报告，不输出 .FW。

```sh
python3 -m leica_source rebuild-reference --input /path/to/M10-4.26.36.10-Customer.FW --output-dir build/reference
python3 codec/independent_verify.py build/reference/M10-4.26.36.10.rebuilt.reference.analysis.bin
```

既有完整参考输入/输出均为上述 127615696 B/hash，135899932 个解压符号；
本源码发布验证只重跑小型合成 codec 测试，不重复昂贵的完整包重建。
旧独立检查的私有逐行索引没有分发；便携检查保留独立 decoder/TOC 检查和完整包/解压 payload pin。

## ASC 本地资源与胶片数据

本节高级命令只适用于包含相应 ASC 或 film 模块的源码包；相关示例会随模块一同打包。
若当前样式是 ASC 或 slide，运行冻结配置仍需提供下列本地资源。

ASC 使用官方普通资源和官方 ASC 资源的已接受组合，不是普通 FW 中现成的功能开关。
另从 [Leica 原站取得 ASC 原包](https://leica-camera.com/sites/default/files/M10-P-ASC-4.26.36.10-Customer.FW)：
121740584 B，SHA256 `aacf21aac8aa1cd1d82c281c5fa1092c158e0bd491d23d9a0ae5197d08c32463`。
ASC 原包仅是本地资源输入，不能刷到普通 M10。

```sh
python3 -m leica_source extract-asc --official-fw /local/M10.FW --asc-fw /local/ASC.FW --output-dir build/asc-inputs
python3 -m leica_source prepare-asc --ordinary-b2y build/asc-inputs/ordinary-b2y.host-resource --asc-b2y build/asc-inputs/asc-b2y.host-resource --output build/asc.host-resource
python3 -m leica_source run-reference --config examples/asc-classic.json --asc-resource build/asc.host-resource
```

提取只复用现有 decoder；合成只复用已有 `parse/encode/hybrid`，输出 1,678,832 B、
SHA256 `0ed4a05cc5b74c07108c0d7e6ab531a7a86a798420a246e2dafa8a1da1efcaf3`。
资源不会进入公开 ZIP，也不是更新包。没有输入时明确拒绝，不生成假的 ASC 图像。

800T 可直接运行。18 款 GRKit 配方需用户从 [GRKit 原 catalog](https://grkit.app/effects/gr3-2.10/catalog.json)
保存本地 JSON（接受整个 catalog 的 `base_gamma` 或该数组）。精确 256×BE16 hash：
`2c8f21661161934be5b419e613dca60b9c2b9ba7eafec747c0afdbaf3077ae0f`。
该表单独的再分发许可未建立，故没有打包或用通用 gamma 替代。

```sh
python3 -m leica_source run-reference --config examples/velvia.json --gamma /local/catalog.json --rgb 120,80,40
# 同一主机入口求值全部 19 款胶片；不执行 NORMAL/ASC 像素替身
python3 -m leica_source source-eval --style all --gamma /local/catalog.json --rgb 120,80,40
```

## 检查与扩展

```sh
python3 -m unittest discover -s tests -v
node --test codec/test_builder.mjs
```

测试覆盖严格配置、依赖闭合、实际 C 调用与失败分支、主机配置快照、确定性 ZIP、
公开内容边界及 `.FW` 拒绝；codec 复用原 24 项合成测试。touch 原生包装保留 pointer32
守卫，compile-sources 对它用 ARM-capable clang 作 freestanding 编译；其他对象在主机编译。
无设备或 ARM 执行。三款 slide 的边界、灰阶、饱和色回归使用可选本地精确 gamma；
若同时提供本地原始 GRKit 参考源码，再逐样本比较原始输出。合成 gamma 测试只检查算术与
错误分支，明确不代表精确配方；缺少本地输入时相应测试会明确跳过。本批环境禁止 socket bind，HTTP 检查通过真实 handler 的内存请求/响应
流完成；协调端另外完成了完整源码与最小下载包的实际 localhost 浏览器检查，
确认冻结配置、缺失控件状态及不改选择的再次导出。
可选本地资源验证见 [开发文档](docs/architecture.md)。

[架构与 patch provider 接口](docs/architecture.md) · [贡献](CONTRIBUTING.md) · [路线](ROADMAP.md) · [许可](NOTICE.md)

开发者可以正常实现缺失 native provider。当前拒绝来自尚无真实实现，
不是外部审批 token 服务；未来必须落实准确 preimage、代码区间、ABI/import、加载与恢复证据。
已接受的文件名调查没有给当前用户菜单 SD 路线提供可用替代安装入口；同版本的严格新版本门仍成立。
