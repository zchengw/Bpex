# Bpex

UE5 C++项目 基于Lyra的Locomotion动画蓝图分层架构

[项目exe文件](https://github.com/zchengw/Bpex/blob/e35cd9cdaa1255d9674893b594e084e717fda138/Build.zip)

## 现有功能：
### 爬墙：
- 从地面上墙、从空中上墙
- 从地面下墙、从空中下墙
- 爬到顶端
- 内转角、外转角
- 倾斜斜面、凹凸墙面
  
### 飞行：
- 基于CharacterMovementComponent中的MOVE_Flying实现

### 连招：
- 基于GAS实现
- 输入缓存/预输入

## 演进方向：
- 爬墙的corner case处理
- 爬墙动画优化：IK来实现手脚触碰墙面、速度匹配、距离匹配、同步组来实现动画过渡手脚同步等
- 打击感优化：顿帧、特效等
- 战斗动画上下半身分层，Layered Blend，以此实现边移动边攻击
- 多人网络同步
