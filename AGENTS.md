# OneCloud 项目约定

## 入口和边界

- 项目入口是 `OneCloud.uproject`，使用 UE 5.8。默认地图是 `/Game/Maps/L_ForestLake`，全局模式在 `Config/DefaultEngine.ini` 配置，森林地图通过 World Settings 使用 `BP_ForestLakeGameMode`。
- 自有资源按 `Content/Art`、`Audio`、`Blueprints`、`Input`、`Maps` 分类。美术按对象分组，组内用 `Meshes`、`Materials`、`Textures` 等表示资源用途。素材包与导入模板沿用各自的包路径。
- `Source/OneCloud` 是单个运行时模块。按 `Game`、`Player`、`Enemies`、`Weather`、`UI` 查找相关逻辑。圣湖规则在 `Game/SacredLake`，开场和局内界面分放 `UI/Comic` 与 `UI/Hud`。蓝图配置场景、资产引用和参数。入口索引见 [README](README.md)，职责和运行链路见 [架构](Docs/Architecture.md)，实作步骤见 [学习指南](Docs/Architecture.md#动手读代码)。

## 命名和验证

- 自有资产沿用类型前缀和英文主体名，使用 `类型_主体_用途或变体`，按需要加用途或变体。例如 `L_Oasis`、`BP_Fairy`、`NS_Rain`。有固定顺序的资产用两位编号，例如 `WBP_ComicPage_01_Oasis` 和 `T_Comic_01_Oasis`。世界大纲可用简短的中文名称，同类实例用明确变体和编号。C++ 沿用 Epic 的 PascalCase、类型前缀和 `b` 布尔前缀，`generated.h` 放在对应头文件的最后一个 include。
- 在 UE 编辑器中移动或改名资产。先查引用，再修复对应重定向器。保存并重新打开涉及的地图、蓝图和资源，按改动范围执行 Play。
- 从项目根目录运行本机的编辑器目标构建。改用其他机器时，将下面的引擎路径替换为该机的 UE 5.8 安装路径。

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' OneCloudEditor Win64 Development "-Project=$PWD\OneCloud.uproject" -WaitMutex -NoHotReloadFromIDE
```

- `SourceArt` 和 `SourceAudio` 存放重导入使用的源文件，`Licenses` 存放来源。`Binaries`、`Intermediate`、`Saved`、`DerivedDataCache`、`.vs` 和 `.slnx` 属于生成内容。验证脚本、报告和备份放在系统 Temp。
- 公开仓库只包含源码、配置、文档、许可记录和展示图。`Content`、`SourceArt`、`SourceAudio` 以及参赛提交材料留在本地并由 `.gitignore` 排除；素材许可记录不代表允许再分发素材。清理资产仍须通过 UE 核验引用，不能依据 Git 忽略规则删除。
- 当前目标为 Windows；`AndroidFileServer` 未使用并显式禁用，避免编辑器自动生成并写入共享配置的设备访问令牌。
