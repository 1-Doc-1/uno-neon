import { afterEach, beforeEach, vi } from 'vitest';
import { WebSocketTransport } from './web-socket-transport';

class FakeSocket {
  static readonly instances: FakeSocket[] = [];
  readyState: number = WebSocket.CONNECTING;
  onopen: (() => void) | null = null;
  onmessage: ((event: { data: unknown }) => void) | null = null;
  onclose: ((event: { code: number }) => void) | null = null;
  readonly sent: string[] = [];

  constructor(readonly url: string) {
    FakeSocket.instances.push(this);
  }
  send(data: string): void {
    this.sent.push(data);
  }
  close(): void {
    this.readyState = WebSocket.CLOSED;
  }
  open(): void {
    this.readyState = WebSocket.OPEN;
    this.onopen?.();
  }
  drop(code = 1006): void {
    this.readyState = WebSocket.CLOSED;
    this.onclose?.({ code });
  }
}

describe('WebSocketTransport', () => {
  let transport: WebSocketTransport;

  beforeEach(() => {
    vi.useFakeTimers();
    FakeSocket.instances.length = 0;
    transport = new WebSocketTransport('ws://test/ws', (url) => new FakeSocket(url) as never);
  });

  afterEach(() => {
    transport.disconnect();
    vi.restoreAllMocks();
    vi.useRealTimers();
  });

  it('is open once the socket opens and delivers valid server messages only', () => {
    const received: string[] = [];
    transport.messages.subscribe((m) => received.push(m.type));
    transport.connect();
    expect(transport.status()).toBe('connecting');

    const socket = FakeSocket.instances[0];
    socket.open();
    socket.onmessage?.({ data: '{"v":1,"type":"ack","replyTo":"c-1"}' });
    socket.onmessage?.({ data: '{"v":1,"type":"mystery"}' });
    socket.onmessage?.({ data: 'not json' });

    expect(transport.status()).toBe('open');
    expect(received).toEqual(['ack']);
  });

  it('reconnects with a delay that doubles from 500 ms up to 8 s, with a jitter of one half to one', () => {
    // The shortest jitter (half of the ceiling): the delays are exactly 250, 500, 1000, 2000, 4000, then 4000 again
    vi.spyOn(Math, 'random').mockReturnValue(0);
    transport.connect();
    FakeSocket.instances[0].open();

    const shortest = [250, 500, 1000, 2000, 4000, 4000, 4000];
    shortest.forEach((delay, index) => {
      FakeSocket.instances.at(-1)?.drop();
      expect(transport.attempt()).toBe(index + 1);
      const before = FakeSocket.instances.length;
      vi.advanceTimersByTime(delay - 1);
      expect(FakeSocket.instances).toHaveLength(before);
      vi.advanceTimersByTime(1);
      expect(FakeSocket.instances).toHaveLength(before + 1);
    });

    FakeSocket.instances.at(-1)?.open();
    expect(transport.attempt()).toBe(0);
    expect(transport.status()).toBe('open');
  });

  it('never waits longer than the ceiling, whatever the jitter', () => {
    vi.spyOn(Math, 'random').mockReturnValue(0.999);
    transport.connect();
    FakeSocket.instances[0].open();

    for (let attempt = 0; attempt < 10; attempt++) {
      FakeSocket.instances.at(-1)?.drop();
      const before = FakeSocket.instances.length;
      vi.advanceTimersByTime(8000);
      expect(FakeSocket.instances).toHaveLength(before + 1);
    }
  });

  it('does not reconnect when another connection took the session over', () => {
    transport.connect();
    FakeSocket.instances[0].open();
    FakeSocket.instances[0].drop(4000);
    vi.advanceTimersByTime(60_000);

    expect(transport.status()).toBe('closed');
    expect(FakeSocket.instances).toHaveLength(1);
  });

  it('refuses to send while the socket is not open', () => {
    transport.connect();

    expect(transport.send({ v: 1, id: 'c-1', type: 'room.leave', payload: {} })).toBe(false);
  });
});
