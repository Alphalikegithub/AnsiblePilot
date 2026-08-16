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
          <h3 style={{ color: '#38bdf8', margin: '0 0 8px 0', fontSize: '15px' }}>📁 核心功能与亮点特性：</h3>
          <ol style={{ paddingLeft: '20px', lineHeight: '1.8', color: '#cbd5e1', fontSize: '13px', margin: 0 }}>
            <li><strong>主机清单在线预览与快速编辑</strong>：在「基础配置」栏提供「预览/查看」按钮，并在下方控制台新增「主机清单预览」选项卡，支持实时查看、重新读取、在线修改保存与一键复制。</li>
            <li><strong>自定义与预设双模式支持</strong>：「进程与日志」和「批量命令」支持手敲自定义任意进程名、日志路径与复杂多行命令，也可一键选择预设快速回填。</li>
            <li><strong>主机清单持久记忆防重置</strong>：刷新扫描自动动态扫描工作目录下所有 <code>*hosts*</code>、<code>*inventory*</code>、<code>*.ini</code> 清单，并精准保留用户当前选择的文件（如 <code>product_hosts</code>）。</li>
            <li><strong>XML 统一配置管理</strong>：配置文件统一存放在 <code>config/cluster_config.xml</code> 中，包含热备对、进程关联以及常用启动脚本三大模块。</li>
          </ol>
        </div>
      </div>
    </div>
  );
};

ReactDOM.createRoot(document.getElementById('root')!).render(<App />);
