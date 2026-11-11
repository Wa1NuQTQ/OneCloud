# 森林圣湖资源引用

## 美术

- 森林地面、蕨类、苔岩、倒木和冷杉幼树来自 Poly Haven 的 CC0 素材库，游戏资产位于 `Content/Art/ForestLake`，重导入源文件归入 `SourceArt/ForestLake`。 [Forest Ground 04](https://polyhaven.com/a/forest_ground_04) · [Fern 02](https://polyhaven.com/a/fern_02) · [Rock Moss Set 01](https://polyhaven.com/a/rock_moss_set_01) · [Dead Tree Trunk](https://polyhaven.com/a/dead_tree_trunk) · [Fir Sapling Medium](https://polyhaven.com/a/fir_sapling_medium)
- 自由种植使用同一 CC0 冷杉的专用网格 `Art/ForestLake/Plants/Meshes/SM_PlantingTree`。原 glTF 的树根离原点约五米，会在缩放时侧移；专用副本通过 UE 将树根移到原点，材质仍引用已有贴图，环境里原有的树不受影响。游戏按模型高度缩放到一级约4.3米，再用根部固定的缩放表现扎根和升级。
- 湖心岛圣树采用 Jagobo 的 **Mighty Oak Trees** 第三株，许可为 CC BY 4.0。项目从原模型提取单株，重定位树根枢轴并缩放至 22 米，保留树皮和叶片贴图，为 UE 配置 Nanite 和双面遮罩叶片。可重导入的模型位于 `SourceArt/ForestLake/SacredOak`，游戏资产位于 `Content/Art/ForestLake/SacredOak`。 [Mighty Oak Trees](https://sketchfab.com/3d-models/mighty-oak-trees-4f6ab5594a8a415aba3f958682b9ced5) · [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- 圣树的隐藏树冠查询网格 `Art/ForestLake/Physics/SM_SacredCanopyQuery` 是项目使用简单球形缩放得到的碰撞代理，树干阻挡使用 UE 胶囊组件；二者服务于落雨查询与飞行阻挡，不是外部树模型自带的精细叶片碰撞。调整入口见[圣树配置](Scene.md#森林圣湖)。
- 场景使用 Unreal 的 Landscape、Volumetric Cloud、雾和导航。森林布局与性能参考 Epic 的小型森林案例。 [Quixel Megaplants](https://www.fab.com/sellers/Quixel%20Megaplants?lang=en) · [Electric Dreams](https://dev.epicgames.com/documentation/unreal-engine/electric-dreams-environment-in-unreal-engine)
- 白天版湖水 `MI_ForestWater` 基于工程原有的 UE 单层水材质实例，按森林关卡调整吸收、散射和粗糙度。外林复用 English Oak，地被沿用现有矮草与 Poly Haven 蕨类。森林地图的 `M_ForestFern` 复用原蕨叶贴图，通过遮罩保留叶片轮廓。
- `T_ForestGroundRegions` 和 `SM_ForestHorizon` 为本地图的地表配色与远山底形。源文件分别位于 `SourceArt/ForestLake/forest_ground_04` 和 `SourceArt/ForestLake/Backdrop`。
- 精灵、蜘蛛、重甲与首领来自工程已有资产。森林首领 `BP_ForestWarden` 复用 Epic 的 Paragon Dawn 超级兵模型与动画。角色许可与来源在 [素材索引](../Licenses/AssetSources.json) 和 [怪物索引](../Licenses/Monsters/Sources.json) 查询。森林开场的七张漫画页为 OpenAI 内置图像生成工具的作品，共三十格，依据 `L_ForestLake` 的实机机位和本项目分镜绘制。生产源文件在 `SourceArt/UI/Comic/ForestLake/Sheets`，游戏贴图在 `Content/Art/UI/Comic/ForestLake/Textures`。[Generation.json](../SourceArt/UI/Comic/ForestLake/Sheets/Generation.json) 记录各页的提示词、修订与实际 UV 区域，供重导入和调整画格时核对。

- 森林降水使用 `NS_ForestRain`、`NS_ForestSnow` 和 `NS_ForestHail`。雪花和冰粒使用项目自制的 Substrate Unlit 精灵材质与轮廓遮罩，三个系统均使用 `User.ForestSpawnRate` 调节密度。 [Epic Substrate 材质](https://dev.epicgames.com/documentation/unreal-engine/overview-of-substrate-materials-in-unreal-engine?application_version=5.8)
- 视觉基础发射率配置为1500个粒子/秒，由同一成长预算分配全云背景与局部天气；G=0/6/18时总配置发射率为1500/1860/2580。三套系统导出接口均为`PerParticle`、系数1，沿用当前1000的导出缓冲上限。粒子数量负责视觉与逐滴落点声音；森林基础伤害每秒3.5点、基础耗水每秒1点及成长倍率保持原规则。配置保存和编译检查不代表帧率、完整落点回读与混音已全部验收。
- 项目自有的 `Art/Weather/Rain/Impacts/M_RainSpray` 与 `M_LakeRipple` 使用 UE 的 `EyeAdaptationInverse` 补偿自发光曝光，修正物理日光下水花和波环过暗的问题。仅改变 RGB 到 Emissive 的连接，保留透明度、时序、实例数据与原尺寸；这是现有材质修正，没有引入新的外部素材。
- 主动天气技能的五个 Niagara 系统位于 `Content/Art/Weather/Skills`：`NS_Weather_WaterColumn`、`NS_Weather_SnowScreen`、`NS_Weather_HailImpact`、`NS_Weather_StarWarning`、`NS_Weather_StarImpact`。它们基于已安装 UE 5.8 Niagara 插件的系统模板调整发射、速度、颜色与尺度；玩法范围、扣费和伤害仍由 C++ 结算。地图中 `圣雨` Actor 的五个 `Presentation` 属性配置粒子、声音和持续时间。雪幕复用雪花材质，其他技能复用项目的 `M_ForestHailParticle` 蓝白水光材质，保持日光下可见。当前为基础的水光喷发、碎片扩散与范围提示，仍可继续提升形状与细节。不是从外部购买的整套技能效果。
- `Blueprints/UI/Menu/WBP_GameMenu` 为项目自有 UMG 菜单。布局、颜色、帮助及署名内容保存在 Designer，按钮和设置行为由 `UI/Menu/OneCloudMenuWidget` 处理；`Blueprints/Player/BP_ForestPlayerController` 配置菜单类与混音资源。

- 森林HUD与祝福UI是项目自有UMG资产：`Blueprints/UI/WBP_SacredLakeHUD`保存固定布局，`Blueprints/UI/Hud/WBP_ForestSkill`保存单槽，`Blueprints/UI/Blessing/WBP_ForestBlessing`保存三选一卡片。白色透明底图标位于`Art/UI/Hud/Textures`，重导入PNG在`SourceArt/UI/Hud`。湖水、云、幼苗、首领、雨雪雹、侧掠与状态图标采用 Lucide 0.468.0（ISC，保留其中 Feather 的 MIT 署名），原SVG与许可分别在`SourceArt/UI/Hud/Svg`及`Licenses/UI`。水柱、星坠、无目标轮廓沿用项目原图形。固定界面通过Designer调整，空间圆环与岸向标记使用原生投影。

## 音频

- 森林风、精灵翼声和远雷为 BigSoundBank 的 CC0 录音剪辑。 [Wind in the Trees](https://bigsoundbank.com/forest-wind-in-the-trees-s0904.html) · [Flight Bird #2](https://bigsoundbank.com/flight-bird-2-s2892.html) · [Thunder #2](https://bigsoundbank.com/tonnerre-2-s3113.html)
- 森林版的水面、叶片和金属根 Cue 使用各自 24 个项目原创单滴音色，沙地沿用原创音色。许可未明的旧录音已从这三个发布 Cue 剔除，保留 Cue 路径、SoundClass、衰减和并发设置，各表面 Cue 的音量独立平衡。开场共用《A Kind Of Hope》，音乐许可见 [OpeningMusic.txt](../Licenses/Music/OpeningMusic.txt)。
- 森林版的开局准备、祝福选择与胜负阶段使用 Scott Buckley 的《Borealis》，战斗开始时淡出。祝福选择使用同一资源的低音量 UI 播放通道，在暂停中继续；连续奖励共用一段，全部选完后淡出，不在战斗中常驻。 [Borealis](https://www.scottbuckley.com.au/library/borealis/) · [许可和署名](../Licenses/Music/ForestInterlude.txt)
- `SW_ForestFirstSiphon` 是工程自制的单声道取水提示音，使用合成水流和气泡。`SA_ForestFirstSiphon` 配置方向和衰减，原始 WAV 位于 `SourceAudio/ForestLake`。
- 新增湿润皮肤落点和天气技能音色使用 rubberduck 的 [40 CC0 water / splash / slime SFX](https://opengameart.org/content/40-cc0-water-splash-slime-sfx)，许可为 [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)。已核对资源页作者与许可。选用的原录音保留在 `SourceAudio/Weather/Originals`；皮肤八个短音和随机 Cue 在 `Audio/Rain/Impacts/Flesh`，技能声在 `Audio/Weather`。处理包括单声道转换、单次瞬态裁切、滤波、首尾淡化与响度匹配；冰雹和星坠还混入已有金属、雷声素材，雪幕和预警包含项目合成音。新增包的 CC0 许可只覆盖该包原录音。
- `Audio/Mix/SCL_Music`、`SCL_Effects` 和 `SMX_Player` 分别供音乐、音效分类与玩家混音。总音量乘入两类音量，退出世界时移除该混音，避免修改共享音频设备的全局音量。逐滴落点发声路径保留，短音仍使用位置与距离衰减。
- 逐滴播放指每个已导出、CPU查询有效且配置了音色的落点独立提交声音请求。声音资产的并发设置、衰减和设备混音预算仍会影响实际声部；音频请求计数与客观容量检查不能代替听感验收。

内容目录根部25个旧Metal、Leaf、Water录音不参与当前正式Cue的选声，许可索引仍保留历史来源待确认状态。是否进入本轮交付包必须在本轮Cook后重新检查容器清单，不能沿用旧包结果。新增CC0包的许可只覆盖对应原录音。逐滴发声路径保留，最终主观混音需人工试听。
