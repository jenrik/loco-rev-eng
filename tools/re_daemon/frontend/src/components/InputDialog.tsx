import { useCallback, useEffect, useRef, useState, type FormEvent } from 'react';

export interface DialogOption {
  label: string;
  value: string;
}

export interface DialogField {
  name: string;
  label: string;
  type?: 'text' | 'textarea' | 'select';
  defaultValue?: string;
  placeholder?: string;
  required?: boolean;
  options?: DialogOption[];
  rows?: number;
}

export interface InputDialogOptions {
  title: string;
  description?: string;
  fields: DialogField[];
  submitLabel?: string;
  danger?: boolean;
}

export type DialogValues = Record<string, string>;

type InputDialogRequest = InputDialogOptions & { id: number };

export function useInputDialog() {
  const [request, setRequest] = useState<InputDialogRequest | null>(null);
  const resolver = useRef<((values: DialogValues | null) => void) | null>(null);
  const nextId = useRef(0);

  useEffect(() => () => {
    resolver.current?.(null);
    resolver.current = null;
  }, []);

  const openInputDialog = useCallback((options: InputDialogOptions) => {
    resolver.current?.(null);
    return new Promise<DialogValues | null>((resolve) => {
      resolver.current = resolve;
      setRequest({ ...options, id: ++nextId.current });
    });
  }, []);

  const resolveInputDialog = useCallback((values: DialogValues | null) => {
    const resolve = resolver.current;
    resolver.current = null;
    setRequest(null);
    resolve?.(values);
  }, []);

  return { request, openInputDialog, resolveInputDialog };
}

export function InputDialog({
  request,
  onResolve,
}: {
  request: InputDialogRequest | null;
  onResolve(values: DialogValues | null): void;
}) {
  const dialogRef = useRef<HTMLDialogElement>(null);

  useEffect(() => {
    const dialog = dialogRef.current;
    if (!dialog) return;
    if (request && !dialog.open) dialog.showModal();
    if (!request && dialog.open) dialog.close();
  }, [request]);

  const submit = (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const formData = new FormData(event.currentTarget);
    const values: DialogValues = {};
    for (const field of request?.fields ?? []) {
      values[field.name] = String(formData.get(field.name) ?? '').trim();
    }
    onResolve(values);
  };

  return (
    <dialog
      ref={dialogRef}
      className="input-dialog"
      aria-labelledby="input-dialog-title"
      aria-describedby={request?.description ? 'input-dialog-description' : undefined}
      onCancel={(event) => { event.preventDefault(); onResolve(null); }}
      onClick={(event) => { if (event.target === event.currentTarget) onResolve(null); }}
    >
      {request && (
        <form key={request.id} className="input-dialog__form" onSubmit={submit}>
          <header>
            <h2 id="input-dialog-title">{request.title}</h2>
            <button type="button" className="input-dialog__close" aria-label="Close dialog" onClick={() => onResolve(null)}>×</button>
          </header>
          {request.description && <p id="input-dialog-description">{request.description}</p>}
          <div className="input-dialog__fields">
            {request.fields.map((field, index) => (
              <label key={field.name}>
                <span>{field.label}</span>
                {field.type === 'textarea' ? (
                  <textarea
                    name={field.name}
                    defaultValue={field.defaultValue}
                    placeholder={field.placeholder}
                    required={field.required}
                    rows={field.rows ?? 4}
                    autoFocus={index === 0}
                  />
                ) : field.type === 'select' ? (
                  <select name={field.name} defaultValue={field.defaultValue} required={field.required} autoFocus={index === 0}>
                    {field.options?.map((option) => <option key={option.value} value={option.value}>{option.label}</option>)}
                  </select>
                ) : (
                  <input
                    name={field.name}
                    defaultValue={field.defaultValue}
                    placeholder={field.placeholder}
                    required={field.required}
                    autoFocus={index === 0}
                  />
                )}
              </label>
            ))}
          </div>
          <footer>
            <button type="button" onClick={() => onResolve(null)}>Cancel</button>
            <button type="submit" className={request.danger ? 'danger' : 'primary'}>{request.submitLabel ?? 'Submit'}</button>
          </footer>
        </form>
      )}
    </dialog>
  );
}
