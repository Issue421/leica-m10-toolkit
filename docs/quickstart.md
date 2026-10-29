# 五分钟上手 —— 全程离线，不需要相机

本页每条命令都在当前这份树上实际跑过。需要 Python 3.10+，其中两条需要 Node.js 20+。
**不需要相机、不需要下载固件、不需要联网。**

需要你自己提供文件的步骤会明确标出 **[需要你自己的文件]**；其余命令在干净 clone 上直接可用。

---

## 1. 确认工具链自身一致（约 5 秒）

```sh
python3 -m unittest discover -s tests
```

预期：`Ran 27 tests` 与 `OK (skipped=13)`。

13 个 skip 是设计好的，不是失败。具体分成两类：

- **8 个**因为 `modules/identity/` 没有发布。它们的门是
  `skipUnless(all((ROOT/'modules'/m).exists() for m in ('touch','identity','framing','wb','asc','film')))`；
  建一个空的 `modules/identity/` 目录就会让它们不再跳过（然后它们会失败，因为
  identity 源码确实不在——这正是重点）。
- **5 个**因为本地输入不再分发：精确 GRKit gamma（3 个）与官方 B2Y 资源（2 个）。

想加"不需要这些输入也能跑"的测试，见 `docs/contributing-ideas.md` 第 A 节。

## 2. 求值一款胶片风格，无需任何输入（约 1 秒）

800T 是闭式配方，不依赖外部数据：

```sh
python3 -m leica_source source-eval --style cinestill800t --rgb 120,80,40
```

输出里会有 `rgb8` 结果和声明的色彩域（`FRAME800T software recipe, RGB8 code input`）。
这些数值是真实计算出来的，来源是 `modules/film/frame800t.py`。

18 款 GRKit 配方需要一份你必须自己提供的本地文件：

**[需要你自己的文件]** ```sh
python3 -m leica_source source-eval --style all --gamma /local/catalog.json
```

不带 `--gamma` 时命令会明确拒绝，而不是拿假数据替代——这是有意为之。文件应是一个
JSON 数组（或含 `base_gamma` 的对象），256 个 `0..16384` 的整数，其大端 u16 打包结果
的哈希等于 `README.md` 里记录的那个精确值。

## 3. 用容器自己的测试校验 codec（约 1 秒）

```sh
node --test codec/test_builder.mjs
```

预期：`pass 24`、`fail 0`。这些用合成样本检验 COMP/TOC 解码器，含反例
（校验和错误、块重叠、截断）。

## 4. 读一份配置，再跑主机参考实现（约 2 秒）

```sh
python3 -m leica_source default-config --output selection.json
python3 -m leica_source validate selection.json
python3 -m leica_source run-reference --config selection.json --rgb 120,80,40
```

`validate` 打印归一化后的配置；`run-reference` 编译并运行主机 C 参考实现，按模块打印
JSON。请确认输出里有 `"scope": "HOST reference execution; not camera qualification or firmware"`
和 `"firmware_available": false`。

这是**主机**运行：不碰相机，也不产生固件。

## 5. 看符号提取器怎么工作（瞬时）

```sh
python3 tools/m10sym/extract.py --help
```

真正干活需要一份 system image。条目格式记录在 `docs/symbol-table.md`；给它加一个
不依赖任何 image 的测试，见 `docs/contributing-ideas.md` 第 A3 节。

## 6. 构建开发源码包（约 2 秒）

```sh
python3 -m leica_source build-source --config selection.json --output development-source.zip
```

预期：`"artifact_class": "development-source"`、`"firmware": false`。
这个 ZIP 只是源码——不要拷到 SD 卡上。

---

## 干净 clone 下做不到的事

以下都需要只有你才有的文件。它们会给出明确拒绝，这是预期行为：

| 命令 | 需要 |
|---|---|
| `run-reference --config … --gamma …`（任何 GRKit 风格） | 你本地的精确 gamma 数组 |
| `run-reference --config … --asc-resource …` | 你本地合成的 B2Y 资源 |
| `extract-native` / `rebuild-reference` / `verify-official` | 你自己的官方固件包 |
| `package-*`、`models`、`identify` | 本树未随附的模块——CLI 会显式说明并指向 `codec/` |

## 接下来读什么

- `docs/status.md` —— 四项功能各自做到哪一步、还缺什么。
- `docs/contributing-ideas.md` —— **不需要硬件**的具体任务，每条带入口文件与验收标准。
- `docs/architecture.md` —— 各部分怎么拼起来。
