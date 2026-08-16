import React from 'react';
import ReactDOM from 'react-dom/client';

const App: React.FC = () => {
  return (
    <div style={{ padding: '24px', fontFamily: 'sans-serif', backgroundColor: '#0f172a', color: '#f8fafc', minHeight: '100vh' }}>
      <h2 style={{ color: '#38bdf8' }}>AnsiblePilot - Qt C++ 原生工程模式已完全恢复</h2>
      <p style={{ color: '#94a3b8', lineHeight: '1.6' }}>
        已完全还原为标准的 <code>MainWindow.ui</code>（Qt Designer 可视化拖拽组件设计）+ <code>MainWindow.h</code> + <code>MainWindow.cpp</code> 架构。
      </p>
      <div style={{ marginTop: '16px', padding: '16px', backgroundColor: '#1e293b', borderRadius: '8px', border: '1px solid #334155' }}>
        <h3 style={{ color: '#4ade80', margin: '0 0 8px 0' }}>本地 Qt Creator 打开与编辑说明：</h3>
        <ol style={{ paddingLeft: '20px', lineHeight: '1.8', color: '#cbd5e1' }}>
          <li>直接在 Qt Creator 中打开 <code>AnsiblePilot.pro</code>。</li>
          <li>双击左侧项目树中的 <code>MainWindow.ui</code>，即可直接在 Qt Designer 图形设计器中自由拖拽、移动、修改组件排版。</li>
          <li>所有核心逻辑均通过 <code>ui-&gt;组件名</code> 直接绑定，完全符合标准的 Qt C++ 规范。</li>
        </ol>
      </div>
    </div>
  );
};

ReactDOM.createRoot(document.getElementById('root')!).render(<App />);
