import { useEffect, useRef } from 'react';

export interface ContextMenuItem {
  label: string;
  action?: () => void;
  danger?: boolean;
}

export interface ContextMenuState {
  x: number;
  y: number;
  items: ContextMenuItem[];
}

export function ContextMenu({ menu, onClose }: { menu: ContextMenuState | null; onClose(): void }) {
  const ref = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (!menu) return;
    const close = (event: MouseEvent) => {
      if (!ref.current?.contains(event.target as globalThis.Node)) onClose();
    };
    document.addEventListener('mousedown', close);
    return () => document.removeEventListener('mousedown', close);
  }, [menu, onClose]);
  if (!menu) return null;
  return (
    <div ref={ref} className="context-menu" style={{ left: menu.x, top: menu.y }}>
      {menu.items.map((item) => (
        <button
          key={item.label}
          className={item.danger ? 'danger' : ''}
          disabled={!item.action}
          onClick={() => { item.action?.(); onClose(); }}
        >
          {item.label}
        </button>
      ))}
    </div>
  );
}
