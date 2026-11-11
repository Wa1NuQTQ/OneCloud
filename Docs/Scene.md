# 场景与美术

圣湖版的湖岸、仪式位置与来袭方位见[场景美术设计](Scene.md#森林圣湖)。

## 森林圣湖

打开 `Maps/L_ForestLake` 再按 Play。地图使用 `BP_ForestLakeGameMode` 播放[森林版开场](OpeningDesign.md#森林圣湖版重制分镜)，读完或跳过后接回精灵视角。湖盆使用可编辑 Landscape，岛心偏东南。岛西是圣树和仪式区，岛东是起降与种植区。外围林带、湖岸的近处植物和雾后背景分开布置。怪物从整圈外围林带随机出雾，北、东、南、西的地标用于辨认方位。

| 位置 | 实际用法 | 美术调整方向 |
|---|---|---|
| 北侧林隙 | 精灵出生时可见的北岸地标 | 高树夹出暗口，保留通往水线的干地 |
| 东侧浅湾 | 辨认东岸和浅水区 | 香蒲和湿岩形成不对称岸边轮廓 |
| 南侧苔岸 | 辨认开阔南岸 | 留开阔低地，苔岩和草丛沿两边聚集 |
| 西侧倒木 | 辨认西岸与倒木区 | 倒木指向岸边，根盘与苔岩形成低剪影 |
| 岛东 | 出生、种植和起降 | 可自由种植的高地优先进入视线，保留回身空间 |
| 岛西仪式石 | `玩法/资源/补云点` 位于立石旁 | 精灵可在 650 厘米空间半径内悬停并按住 **F** 补云 |

`相机_总览` 检查湖盆、岛和四岸，`相机_出生` 检查精灵肩后视线，`相机_岸边仰视` 检查云腹与近岸。`玩法/森林圣湖 · 规则` 配置水量、出雾环带、种植岛界、补云点、精英与首领计时及 HUD。`ForestSpawnHalfExtentCm` 控制以湖心为中心的椭圆环带半轴，默认 `(6800, 6250)` 厘米；候选角度连续随机，沿径向再浮动约正负3米。地面须干燥，能投影到 NavMesh，并有完整干路通往当前水线，才能生成怪物。旧 `SpawnPoints` 不再参与森林出怪，仍供单怪原型地图使用。调整水面、环带或岸坡后，要一起检查落点查询、种植地高度和 NavMesh，尤其是低水退后的新岸。

开局实际镜头由场景中的 `精灵` 和它的 `CameraArm` 决定。圣树在画面右侧，可种植的空地在左前方，湖面反光延伸到北岸林隙。`相机_出生` 与开局肩后视角对齐，可用于漫画最后一格。`北岸林隙石` 是来向地标，仪式石与伴石嵌进岛西坡面。云水剩一半或更少且仍可补水时，补云图标标出仪式石，转开镜头后由屏幕边缘箭头指示方向。进入范围后才显示 **F**。

`湖心岛/圣树 · 古橡` 使用 `Art/ForestLake/SacredOak/StaticMeshes/SacredOak`。树高 22 米，树冠在网格局部坐标约 32 × 30 米。树根略埋入岛西地面，沿 Z 轴转 169°，让树干在出生画面右侧，同时留出岛东的视线与起降通道。古橡启用 Nanite，叶片使用双面遮罩材质。 [古橡来源与许可](Assets.md#美术)

圣树的可见模型、落雨查询和角色阻挡分别配置。`湖心岛/圣树/圣树 · 树冠落雨` 用隐藏的椭球网格 `Art/ForestLake/Physics/SM_SacredCanopyQuery`，仅阻挡 `Visibility` 查询并使用 `PM_Leaf`，不挡精灵、不参与导航。`圣树 · 树干碰撞` 用胶囊阻挡 `Pawn` 和 `Camera`，防止角色穿过树干。它们是独立 Actor；移动或缩放可见圣树后，要一起调整这两个代理并复查雨声、飞行与镜头，世界大纲放进同一文件夹不会自动同步变换。

森林版采用白天斜照。大纲 `天光与天气` 下的 `日光`、`晴空`、`林间天光` 和 `日景曝光` 分别调整直射光、天空、环境反光和曝光。`北岸林隙补光` 只提亮入口附近，局部低雾退到外林。湖水用森林专属 `MI_ForestWater`，浅岸可见湖底，深水保留蓝绿色倒影。水面组件关闭 `Use Default Collision`，使用 `Custom / Query Only`，仅 `Visibility=Block`、其余通道均为 `Ignore`，物理材质为 `PM_ForestWater`，不影响导航。雨滴仍可查询湖面，精灵和怪物不会被水面阻挡或把它当作地板。云底与降雨范围沿用原关卡配置。

白天曝光也会压暗 Unlit 特效。水花和波纹的 `M_RainSpray / M_LakeRipple` 已对 RGB 加入 `EyeAdaptationInverse`，透明度、时间和原尺寸不变；调整光照后先从水面近景检查可读性，再看正常飞行视距，不通过抬高波环或关闭湖水来掩盖问题。当前近景 A/B 与材质重载检查不代表所有机位都已验收。

`森林/深林树冠` 的阔叶组填在原有杨树林之间，外林分成四片大小不同的树群，北、东、南、西保留来袭缺口；冷杉在湿岸后方补出层次。`森林/远山与林线` 延续湖盆以外的地势和远景。远山网格没有碰撞，也不参与导航。岸边矮草、蕨丛和半埋苔岩以不对称斑块布置，地被使用实例化组件，给四侧出雾方向留出空地。岛岸、仪式区和外岸的岩石沿 Landscape 碰撞面落位，斜坡上的石脚适当埋入土里。远林沿山脊成簇补密，从空中看更容易分辨湖盆和森林的层次。`M_ForestGround` 通过 `T_ForestGroundRegions` 控制林下、湿岸和步道的色彩差异，原有动态湿岸仍由水位驱动。外林最外侧的地表颜色逐渐过渡到远山材质的顶点色，减少湖盆方形边缘的色差。岛上不再限制六个预设位置。E进入种植预览后，可在规则Actor的`PlantingIslandCenterXY / PlantingIslandHalfExtentCm`椭圆内选地面，根部须高于最高水面30厘米；角色距落点不超过35米，树根间距至少4.5米，并避开树干与岩石。一局最多六棵、每棵三级；以2.2米根部选择范围培育指定树。种植专用树苗网格以树根为原点，成长只缩放，不移动树根，也不修改环境共享模型。

森林关卡使用已有的精灵、蜘蛛与重甲模型。云使用本图的 `MI_ForestStorm` 和体积材质 `M_ForestStorm`。宽云腹连接三处高低、大小不同的云冠，低处云体过渡连接，云边由两层噪声打散；调整密度形状时保留现有体积采样质量。`天光与天气/湖上雨云` 控制云层的采样高度，材质实例的 `CloudCenter` 和 `CloudHalfExtents` 控制实际体积。当前中心为 `(120,-70,10120)` 厘米，半尺寸为 `(6658.5,5577.5,3662.5)` 厘米；相较原值，水平扩大15%、厚度增加25%，中心上移以保留低云腹。它们是形状归一化参数，并非硬边长方体。云层外壳从64米到188米，容纳树木成长后加高的云冠；视线与阴影采样比例仍为1。

圣雨发射器仍在76米高处，覆盖约105 × 88米，聚雨圈半径12米；云的装饰边缘与实际战场雨区分别配置，放大外形不会摊薄基础雨量。`NS_ForestRain` 的初始化雨线尺寸为2.2 × 70厘米，`M_Rain` 用 `EyeAdaptationInverse` 补偿白天曝光，透明遮罩和原色不变。此次可见性调整不增加发射率，也不改变伤害、耗水与逐滴声音。外林用八个局部雾体衔接，背景林带用三组动态风实例。圣树和 `岛边小树_01 / 02` 的树冠均响应雨滴查询。

当前森林玩法见[玩法与实现](Architecture.md#森林圣湖怎么玩和怎么读)。自由种植区域须在开局、低水和最高水位检查可种、根部贴地及飞行通道，并验证被挡、过近、岛外与成长中的目标不扣费；首领生命阈值换岸须核对路线始终在可走干地上。旧场景的历史检查不代替本轮自由种植与长局验收，验收方法见[构建与验证](Architecture.md#构建与验证)。正式入口是`L_ForestLake`，素材许可见[资源引用](Assets.md)。

## 月夜圣湖原型

在 `Content/Maps/L_SacredLake_Prototype` 开始 Play。岛东侧为精灵起点，初始镜头朝北岸来袭通道。前景芦苇勾勒岛岸，湖面和两侧砂岩引向雾边。English Oak 是岛上的圣树，树西侧的立石和两块低石组成仪式区。世界大纲分为 `地形 / 圣湖 / 湖心岛 / 外岸 / 天气与光照 / 玩法 / 相机`。

| 要调整的内容 | 编辑入口 |
|---|---|
| 取景机位 | `相机/相机_总览` 检查湖盆，`相机/相机_出生` 检查北岸构图，`相机/相机_岸石` 检查陶瓮、水面和圣树，`相机/相机_仪式石` 检查树下石组。实际开局的视角由场景精灵的朝向和 `CameraArm` 决定。圣湖关卡实例的 `Socket Offset Y` 为 145 厘米，让北岸的蜘蛛从精灵翅膀旁露出来。 |
| 水量和出生侧 | 选中大纲 `玩法/圣湖规则`，调整 `Initial Water` 与 `Spawn Side`。出生侧编号按北、东、南、西排列。 |
| 水面与湿岸 | `圣湖/动态水面` 复用 `Art/Water/Materials/MI_Lake`。湖床材质位于 `Art/Ground/Lakebed`。 |
| 陶瓮与圣树 | `Art/Props/ClayPitcher`，English Oak 保留 `Megaplant_Library` 原路径。 |
| 降雨与重试 | `Input/SacredLake/IMC_SacredLake`。鼠标右键或 **Q** 切换降雨，点击并松开鼠标中键或 **R** 重试。 |
| 蜘蛛与结果界面 | `Blueprints/Enemies/Spider/BP_SacredLakeSpider` 配置原型生命值并继承蜘蛛动画。`Blueprints/UI/WBP_SacredLakeHUD` 的 Designer 配置布局，`SacredLakeWidget` 更新水量和结果。 |

开局水量 60，低水对照 15，最高 130。水位相对开局的高度分别为 0、−140 和 +120 厘米。湖面的渲染网格和查询碰撞一起升降，地形遮住低于岸线的部分。`MPC_SacredLakeSurface` 把高度和水量传给湖床材质，控制水线附近的湿色与粗糙度。湖水归零时隐藏水面并关闭它的查询碰撞，露出裂泥湖床。雨区由场景中的雨 Actor 配置。

开局水面约 62 × 45 米，高水约 89 × 64 米。岛上台地约 24 × 16 米，坡岸随水位露出。湿色带按实际水线绘制。外岸的 `北岸峡口` 和 `东岸芦苇湾` 用两种不同轮廓区分来向，`雾后山脊` 增加远处层次。岩体以装饰摆设处理，保留岸边接敌空间。草和芦苇集中在岩脚与水湾。月光照向精灵和近岸，仪式石保留一处暖光。

`Source/OneCloud/Game/SacredLake/SacredLakePrototype` 保存水量、选择取水位置并判定胜负。`Source/OneCloud/Enemies/OneCloudEnemy` 沿用 Recast 寻路、攻击动画和受击。蜘蛛先找到一条不跨过水面的路径，停在干岸取水，随后每 3 秒取 4 点。岸边失去可取水范围时重新找点。`Source/OneCloud/Weather/OneCloudRain` 负责逐滴命中、伤害和声音。`Source/OneCloud/UI/Hud/SacredLakeWidget` 根据玩法状态更新界面。原型的规则配置在 `Blueprints/Game/BP_SacredLakePrototype`，场景里的实例连接水面、雨和出生点。

阅读代码时，可以先把 `Initial Water` 调到 15，观察水面和湿岸一起退到低处。对应入口是 `UpdateWater()`。再看 `FindShoreGoal()` 如何用同一个水面高度排除积水路径，最后跟到 `ExtractWater()`，看一次抽水怎样更新水位、重新找岸并刷新界面。界面的位置和样式可直接在 `WBP_SacredLakeHUD` 的 Designer 调整。

首次进入时，共用的 `BP_OneCloudGameMode` 播放漫画，读完后恢复飞行和关卡计时。重试用关卡选项 `SkipIntro` 直接进入玩法。HUD 只在雨、水量、取水或胜负状态变更时更新。右键图示常驻，干涸或守住后出现中键重试图示。

## 原地图 L_Oasis

入口为 `Content/Maps/L_Oasis`。沙漠中的湖心岛是视觉中心：一棵加宽树冠、控制高度的杨树作为精灵住所地标，默认 Fey 和出生点位于树前；岛上辅以少量小树、灌木和草，外湖岸植被成簇，前岸与水面保留视线空间。

## 先看场景

世界大纲按 `光照 / 地形 / 湖水 / 岩石 / 植被 / 天气 / 角色 / 相机 / 起点` 组织。`角色/精灵` 中的“精灵”使用 `Blueprints/Player/BP_Fairy`，基于 `Source/OneCloud/Player/OneCloudFairy`。它使用现有 Fey 模型和循环 Idle，自动由玩家一接管；“出生点”在岛上主树前，供无预放角色时使用。

## 玩家移动与相机

当前预放精灵初始朝向约118.6度，镜头面向湖心岛主树；CameraArm 长度650厘米。世界出生点也朝向主树，飞行与鼠标视角控制保持原样。

湖泊使用官方 Water/Single Layer Water 材质。`MI_Lake` 调整泡沫亮度、吸收、散射和粗糙度，减弱连续的高亮岸边。太阳与环境光的调整入口见[光照](#光照)。

漫画结束或跳过后：WASD 按相机水平朝向移动，空格上升，左 Ctrl 下降，鼠标控制第三人称视角。角色使用引擎 Character Movement 的 Flying 模式，松开按键会减速，不受重力下落；胶囊体负责与场景碰撞。仅接入移动和视角，没有攻击或技能。

`Input/IMC_Fairy` 配置按键，`IA_Move / IA_Fly / IA_Look` 分别表示平移、升降和视角。`BP_Fairy` 中 Character Movement 的 Max Fly Speed 调整飞行速度（厘米/秒，默认600），CameraArm 的 Target Arm Length 调整镜头距离（厘米，基类默认450，当前预放精灵为650），Socket Offset 调整构图。继续使用 UE 默认 PlayerController；角色负责绑定输入，没有新增控制管理器。

### 精灵动画

`Blueprints/Player/ABP_Fairy` 的 AnimGraph 按 `FlightPose` 混合悬停、起步、巡航、停止和转身五种姿态，再叠加 `Wings_Flapping_LSA` 拍翼。`Source/OneCloud/Player/OneCloudFairyAnimInstance` 读取实际速度（厘米/秒，包含升降）与移动加速度，识别输入开始、松开和明显反向。短动作时长读取对应动画资产，播放后回到巡航或悬停；再次输入可中断停止动作。

`Art/Characters/Fairy/Animations/BS_FairyFlight` 在速度0使用原资源 `Idle_Travel`，速度600使用 `Travel_Fwd`，中间平滑混合。角色会转向移动方向，因此当前不需要四向侧飞；升降沿用同一移动姿态，没有冒充专用升降动画。调整混合阈值、样本和 Target Weight Interpolation Speed 时直接编辑 Blend Space；更改 Max Fly Speed 后同步检查横轴最大值。供应商动画原件没有改动。

起步、停止和转身使用 `Art/Characters/Fairy/Animations/A_FlightStart / A_FlightStop / A_FlightTurn`，分别复制自 `Travel_Fwd_Start / Travel_Fwd_Stop / Travel_Fwd_Pivot_180`。副本开启根运动提取并锁定根骨骼，动画实例设为 Ignore Root Motion；角色的位移和朝向由 Character Movement 负责，避免动作重复驱动位移或转向。原始供应商资源保持原样。动画蓝图的按枚举混合节点采用短淡化，并在重新进入动作时重置播放进度。

替换短动作时，同时调整 `ABP_Fairy` 对应的 Sequence Player 和 Class Defaults 中的 `Start Animation / Stop Animation / Turn Animation`，以保持画面和计时一致。转身只在持续移动时出现明显水平反向时触发，普通小角度转向仍由移动组件平滑旋转。

`Wings_Flapping_LSA` 是局部空间 Additive 动画，在 Apply Additive 节点持续叠加到全身动作上；可调该节点 Alpha 改变拍翼强度。资源还包含其他四向 Travel/Jog 与 Jump 系列，尚未加入额外技能或跳跃流程。

输入按 UE 5.8 的 `Default Key Mappings` 保存到 `Input/IMC_Fairy`。此前资产的弃用 `Mappings` 数组虽然包含数据，运行时实际映射为空；已用引擎 MapKey 接口重建。验证应通过 `Input.+key W` 等按键入口检查，而非只向 IA_Move 注入动作值。实际操作仍是 WASD、空格、左 Ctrl 和鼠标；先读完或跳过开场漫画，进入游戏控制。

选择 `相机_总览`、`相机_俯视`、`相机_岸边` 并 Pilot，可以分别检查整体构图、湖岛轮廓和近处材质。`相机_云上`、`相机_云下` 用于检查厚云。编辑器的右键＋WASD观察属于制作工具，当前没有另写游戏自由相机。

| 核心尺寸 | 设置 |
|---|---|
| Landscape | 1009×1009 高度采样、64个组件，XY间距60厘米，约605×605米 |
| 湖泊设计中心 | 世界XY `(-400, 1600)` 厘米，水面Z=0 |
| 外湖岸轮廓 | 基准半轴47×36米，加入低频湾岬变化，沿地形交界连续裁切 |
| 湖心岛轮廓 | 设计中心(-450,1700)厘米，基准半轴14.5×12.5米，岛顶约高于水面2.4米 |
| 主树 | 现有 Poplar A，缩放XY=2.9、Z=1.5 |

半轴是生成轮廓的设计参数，实际网格边界会随岸线和高度采样变化。改变湖、岛、水位后，应一起检查地形、水面、植被、区域遮罩和碰撞。

## 资源从哪里找

| 目录 | 内容 |
|---|---|
| `Art/Ground` | 沙地、土壤、地形材质和碎屑贴花 |
| `Art/Rocks` | Cliff、Mesa、Boulder、Rubble 四套砂岩 |
| `Art/Plants` | Trees、Bushes、Reeds、Grass；Shared保存共用材质和风数据 |
| `Art/Water` | 湖水材质与贴合湖岸、岛岸的水面网格 |
| `Art/Weather` | Clouds 体积云、Rain 雨滴粒子及其 Impacts 表面特效 |
| `Art/Monsters` | 蜘蛛及已有Paragon怪物原包 |
| `Art/UI/Comic/Panels` | 开场漫画的十二格插画 |
| `Art/Shared` | 扫描材质与物理表面材质 |
| `ParagonFey` | Epic精灵原包，内部Audio、Characters、FX路径保留 |

精灵默认模型为 `ParagonFey/Characters/Heroes/Fey/Meshes/Fey_GDC`，外观、骨架、材质和动画来自官方资源。玩家通过项目自己的 `BP_Fairy` 操控岛上的精灵。悬停、起步、飞行、停止、转向与拍翼由 `ABP_Fairy` 混合。

`ParagonFey` 暂保留根路径，避免整理目录时破坏其共享材质、动画、声音、FX和示例蓝图引用。项目自有美术仍按对象分类，再分 `Meshes / Materials / Textures`。PVE植物的 `Graphs / Presets / Parts / Generated` 是编辑和重新生成入口，不能只因地图没有直接引用就删除。

## 地形、湖水与地表

地形使用原生 Landscape。生产高度图为 `SourceArt/Ground/Heightmaps/Oasis/HM_OasisBasin_1009.r16`；原始StampIT输入保存在同级 `StampIT/HM_Dunes_06_Ex.png`。湖盆与岛的形状来自高度数据，湖心最深处约4.2米。高度写入UE 5.8的持久化基础编辑层，完成层合成后重建碰撞；只改合成后的高度贴图会在重新载入时被原编辑层覆盖。

湖水使用官方 Water 插件的原生 WaterBodyCustom，材质实例为 `Art/Water/Materials/MI_Lake`，父材质为官方 `Water_Material_CustomMesh`，使用 Single Layer Water。水面网格 `Art/Water/Meshes/SM_Lake` 依据岸线生成，岛屿范围留空；生产源为 `SourceArt/Water/Meshes/SM_Lake.obj`。网格正面法线朝向+Z，提供水面查询碰撞，不是带湖底的完整水体积。官方材质会在背面关闭体积散射与吸收，不能用双面碰撞代替正确的渲染面朝向。

水体不雕刻Landscape，因此不使用WaterBrushManager；失效的旧笔刷辅助已移除。查询碰撞配置在WaterBodyCustom母组件上，由引擎复制到水面组件：Query Only、阻挡Visibility、PM_Water。水面使用双面复杂查询几何，避免薄平面从一侧漏检。修改高度图后须重建Landscape碰撞，不能只检查可见地形。

湖水近处/掠射粗糙度为 0.24/0.28，材质波纹近处尺度 256 厘米、远处 1800 厘米。`MI_Lake` 的 Absorption 为 (48, 113, 140, 6)，官方材质将 RGB×A 用作吸收距离（厘米）。散射设为 (0.095, 0.205, 0.208, 0.072)，岸边泡沫透明度为 0.09，近处法线强度为 0.18。岸边浅沙自然透出，水深处保留蓝绿色和倒影。降雨命中叠加水花与波环，常驻水面起伏由湖水材质负责。修改 WaterBody 闭合样条后，需要单独更新湖盆与水面网格。 [Epic 单层水材质](https://dev.epicgames.com/documentation/unreal-engine/single-layer-water-shading-model-in-unreal-engine)

`Art/Ground/Terrain/Materials/M_Terrain` 混合两组尺度与方向的沙地扫描贴图，并混合 Forest Floor 土壤。`Art/Ground/Terrain/Textures/T_Regions`（1024×1024）控制岛上和外岸的土壤、窄湿岸与草地适宜度，其生产源为 `SourceArt/Ground/Terrain/Textures/T_Regions.png`。

区域遮罩覆盖世界XY的-10000～10000厘米，UV为 `(WorldPosition.xy + 10000) / 20000`，关闭sRGB并使用Clamp。RGBA分别表示土壤、湿岸、岩体排草区、草地适宜度。它是静态美术遮罩，雨滴不会实时写入湿度。

## 岩石与地表碎屑

砂岩按原有组合落在当前地形或下层岩石上。堆叠岩组一起移动，保留组内相对构图；贴地碎石另外调整倾角、位置和埋入量，避免薄底面跨过坡地形成明显缝隙。天然岩檐允许局部悬挑，接地检查不能只比较Actor原点或包围盒最低点。

不适合当前坡面的零碎扫描片已从关卡移除，原始美术资产仍保留。再次摆放薄扫描模型时，优先选平缓地面，并从低处检查整片底面。

枯枝碎屑是 `Art/Ground/Debris` 的贴花，不是悬空树枝模型。它们分布在干岸，投射方向对齐当地坡面，并使用较浅的投射范围；湖水关闭接收这些贴花，避免碎屑落在水面。之后修改湖岸或地形高度，应重新检查贴花覆盖的整片地面，不能只把中心点下移。调整贴花的尺寸时，保留较浅的投射深度，范围过深会把同一图案投到邻近物体上。

## 植被与风

Megaplants的Poplar、Greasewood、Bulrush和Dry Reed沿用现有模型。381株植物保存在11组原生 InstancedSkinnedMeshComponent 中；其中主树是Poplar A的一个放大实例。其余树、灌木和芦苇分布在岛岸与外湖岸，按砂岩侧、后岸、右岸等五组成簇，保留裸沙断口和树前出生区；岛上灌木控制为12株，其余归入外岸斑块。8组草HISM保留原有4822个实例，重新分布到岛后和外岸草丛。

11组组件的Transform Provider使用 `Art/Plants/Shared/Wind/DW_Oasis`，植物模型使用官方Dynamic Wind Skeletal Data；风动由UE插件处理。DW_Oasis是共享风变换Provider；风强和方向由Dynamic Wind系统参数决定，植物响应由模型的DynamicWindSkeletalData决定，调整植被分布时修改对应实例。无需为每株树增加独立蓝图和Tick。

Kikuyu Grass与Wild Grass各保留四个LOD0变体，共8组原生HISM。扫描草使用双面遮罩材质、Nanite Preserve Area和遮罩mip覆盖率设置；两套草当前尚未接入风。保留现有材质质量参数，不能仅由“已实例化”推断所有场景负载下都能达到目标帧率。

杨树 A/B 的树冠查询形状同时用于叶片雨声与水花。视觉水花向树冠内侧收，近似落在枝叶上。植被风动使用 `DW_Oasis`，由场景中的实例组共享。相关入口见[降雨说明](Architecture.md#动手读代码)。

## 云与降雨

“云”使用 UE 原生 VolumetricCloud Actor。`Art/Weather/Clouds/Materials/M_Cloud` 用体积噪声塑造绿洲上空的单朵云，轮廓由宽云底和高度错开的云团构成。`MI_Cloud` 保存尺寸、厚度、颜色和密度，当前 DensityScale=0.20。用 `相机_云上` 和 `相机_云下` 检查轮廓及底部厚度。

当前 `CloudCenter=(-400,1600,10000)` 厘米定义云底中心，`CloudHalfExtents=(10000,10000,5000)` 的XY为水平半轴、Z为完整厚度：约200×200米范围、云底100米、厚度50米。移动原生Actor不会自动更新材质中的中心；修改外形请从材质实例开始，并确保原生云层外壳容纳实际密度范围。各视角使用同一厚云，不做俯视透明挖洞。

降雨使用官方Niagara。`Art/Weather/Rain/NS_Rain` 与云材质的范围分别配置，尚未自动联动；改云中心或尺寸后需要同步检查雨区、位置和固定包围盒。C++ `OneCloudRain` 接收粒子回调、查询表面并选择声音，`SelectImpactSound` 集中处理表面分类与选声，仍保留逐滴碰撞发声方式。

## 光照

太阳为暖白，强度 9.2，Pitch −41°，Yaw 126°，光源角 1.35°，Cloud Shadow Strength 为 0.28。出生点的精灵和主树保留正面受光，砂岩和树冠可从岸边辨认。天光使用固定 `Desert_Outer_HDR`，强度 3.35，暖色下半球补光为 (0.155, 0.127, 0.094)。大气 Mie Scattering Scale 为 0.012。指数高度雾从 150 米外开始，最大不透明度 0.16。场景保留原有云采样和画质档位。

## 历史沙漠场景的功能边界

- 场景已有精灵飞行、植被风动、持续降雨、表面声音和命中特效。怪物拥有寻路、攻击动画、雨滴伤害、受击、血条和死亡清理。
- 森林当前的三级种植、湖云水循环、局部天气、精英祝福和首领胜负见[玩法与实现](Architecture.md#森林圣湖怎么玩和怎么读)。历史沙漠部分仅解释旧场景资产，不代表当前地图配置。
- [开场漫画](OpeningDesign.md#现有沙漠原型分镜)围绕湖心岛、主树、精灵和入侵者展开。四页插画等比显示，逐格揭示，支持推进、回看和跳过。

材质、植物、云与声音需要在近景和总览中分别检查；地图和资产能加载不等于画面、风动、音频和性能均已验收。生产源随工程保留，验证脚本、报告、备份和绘画参考放在项目外Temp，运行不依赖临时目录。

场景摆放与漫画替图不修改运行时代码或 Config。接地检查结合实际网格底面、Landscape 与岩石支撑查询，并从低处近景和固定总览机位复核。开场的播放检查与可调整项见[开场漫画](OpeningDesign.md#现有沙漠原型分镜)。

[返回首页](../README.md)

旧配置的固定机位记录：RTX 4060 Laptop 在 1920×1082、100% 渲染比例、开启声音和命中特效的 PIE 预览中，帧时间中位数约 25–35 ms。此前对比过风动屏幕尺寸阈值、方向光虚拟阴影贴图分辨率、小植被投影和半分辨率湖面反射；收益随机位与采样窗口变化，正式工程保留原有渲染设置。这些历史采样不能代表当前1500粒子/秒、自由种植与完整长局的帧率，当前配置和打包版仍须分别实测。高空云层参考机位可见有限地形的边界。 [Epic 虚拟阴影贴图](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine)
