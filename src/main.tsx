import React from 'react';
import ReactDOM from 'react-dom/client';

const App: React.FC = () => {
  return (
    <div style={{ padding: '28px', fontFamily: 'sans-serif', backgroundColor: '#0f172a', color: '#f8fafc', minHeight: '100vh' }}>
      <div style={{ maxWidth: '960px', margin: '0 auto' }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', borderBottom: '1px solid #334155', paddingBottom: '16px' }}>
          <div>
            <h1 style={{ fontSize: '24px', fontWeight: 'bold', color: '#38bdf8', margin: 0 }}>AnsiblePilot - 集群 XML 统一配置与 UI 联动已就绪</h1>
            <p style={{ color: '#94a3b8', margin: '6px 0 0 0', fontSize: '14px' }}>
              基于 <code>config/cluster_config.xml</code> 统一管理热备对、进程查询、日志路径与启动脚本
            </p>
          </div>
          <span style={{ backgroundColor: '#0369a1', color: '#e0f2fe', padding: '4px 12px', borderRadius: '9999px', fontSize: '12px', fontWeight: 600 }}>
            Qt5 / C++ 原生架构
          </span>
        </div>

        <div style={{ marginTop: '24px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
          <div style={{ backgroundColor: '#1e293b', padding: '16px', borderRadius: '8px', border: '1px solid #334155' }}>
            <h3 style={{ color: '#4ade80', margin: '0 0 10px 0', fontSize: '16px' }}>🚀 进程与日志快速联动</h3>
            <ul style={{ paddingLeft: '20px', color: '#cbd5e1', lineHeight: '1.7', fontSize: '13px', margin: 0 }}>
              <li><strong>实时数据交换 (Pair 1)</strong>: 进程 <code>dods</code> / 日志 <code>/root/work/dods.log</code></li>
              <li><strong>外测数据处理 (Pair 2)</strong>: 进程 <code>EMDPS</code> / 日志 <code>/root/work/EMDPS.log</code></li>
              <li><strong>GNSS差分处理 (Pair 3)</strong>: 进程 <code>gnss</code> / 日志 <code>/root/work/gnss.log</code></li>
              <li><strong>遥测预处理 (Pair 4)</strong>: 进程 <code>pre</code> / 日志 <code>/root/work/pre.log</code></li>
              <li><strong>实时综合处理 (Pair 5)</strong>: 进程 <code>RIPS</code> / 日志 <code>/root/work/RIPS.log</code></li>
              <li><strong>遥测全帧处理 (Pair 6)</strong>: 进程 <code>go</code> / <code>mediamtx</code></li>
              <li><strong>指显数据收发 (Pair 7)</strong>: 进程 <code>dataBridge</code> / 日志 <code>/root/work/dataBridge.log</code></li>
            </ul>
          </div>

          <div style={{ backgroundColor: '#1e293b', padding: '16px', borderRadius: '8px', border: '1px solid #334155' }}>
            <h3 style={{ color: '#facc15', margin: '0 0 10px 0', fontSize: '16px' }}>⚡ 常用启动脚本与实用命令</h3>
            <ul style={{ paddingLeft: '20px', color: '#cbd5e1', lineHeight: '1.7', fontSize: '13px', margin: 0 }}>
              <li><code>/root/work/run_dods.sh</code> (启动 DODS 实时数据交换)</li>
              <li><code>/root/work/run_EMDPS.sh</code> (启动外测数据处理)</li>
              <li><code>/root/work/run_go.sh</code> (启动遥测全帧处理)</li>
              <li><code>/root/work/run_pre.sh</code> (启动遥测预处理)</li>
              <li><code>/root/work/run_gnss.sh</code> (启动 GNSS 差分处理)</li>
              <li><code>/root/work/run_RIPS.sh</code> (启动实时综合处理)</li>
              <li><code>/root/work/fix_ptp_route.sh</code> (服务器时间同步校时)</li>
              <li><code>/root/work/deleteSourceData.sh</code> (清空 DODS 历史存储数据)</li>
            </ul>
          </div>
        </div>

        <div style={{ marginTop: '20px', padding: '16px', backgroundColor: '#1e293b', borderRadius: '8px', border: '1px solid #334155' }}>
          <h3 style={{ color: '#38bdf8', margin: '0 0 8px 0', fontSize: '15px' }}>📁 配置文件与代码组织规范：</h3>
          <ol style={{ paddingLeft: '20px', lineHeight: '1.8', color: '#cbd5e1', fontSize: '13px', margin: 0 }}>
            <li>配置文件统一存放在 <code>config/cluster_config.xml</code> 中，包含热备对、进程关联以及常用命令三大模块。</li>
            <li>程序启动或刷新扫描时自动通过 <code>QXmlStreamReader</code> 解析 XML；同时代码内置了健壮的 Fallback 数据保证离线开箱即用。</li>
            <li>在「进程与日志」Tab 下拉选择即可一键填充进程关键字、关联日志（自动指向 <code>/root/work/</code>），并可一键关联 Limit 主机。</li>
            <li>在「批量命令」Tab 下拉选择即可一键填充脚本路径与推荐执行模块，极大提升生产运维操作效率。</li>
          </ol>
        </div>
      </div>
    </div>
  );
};

ReactDOM.createRoot(document.getElementById('root')!).render(<App />);
