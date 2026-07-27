import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import { InputDialog } from '../src/components/InputDialog';

describe('InputDialog', () => {
  it('renders a native dialog with configured form controls', () => {
    const html = renderToStaticMarkup(
      <InputDialog
        request={{
          id: 1,
          title: 'Add task',
          description: 'Describe the work.',
          submitLabel: 'Create',
          fields: [
            { name: 'title', label: 'Title', required: true },
            { name: 'instructions', label: 'Instructions', type: 'textarea', required: true },
            {
              name: 'role', label: 'Role', type: 'select', defaultValue: 'validator',
              options: [{ label: 'Validator', value: 'validator' }],
            },
          ],
        }}
        onResolve={() => undefined}
      />,
    );

    expect(html).toContain('<dialog');
    expect(html).toContain('<form');
    expect(html).toContain('<textarea');
    expect(html).toContain('<select');
    expect(html).toContain('Create');
  });
});
