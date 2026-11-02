// Plays the stereo samples the game sends (Float32, interleaved), in order;
// tells the page how many it has played.
class VibAudio extends AudioWorkletProcessor {
  constructor() {
    super();
    this.queue = [];
    this.offset = 0;
    this.played = 0;
    this.blocks = 0;
    this.port.onmessage = (e) => this.queue.push(e.data);
  }
  process(inputs, outputs) {
    const left = outputs[0][0];
    const right = outputs[0][1] || left;
    for (let i = 0; i < left.length; i++) {
      const chunk = this.queue[0];
      if (!chunk) {
        left[i] = right[i] = 0;
        continue;
      }
      left[i] = chunk[this.offset];
      right[i] = chunk[this.offset + 1];
      this.offset += 2;
      this.played++;
      if (this.offset >= chunk.length) {
        this.queue.shift();
        this.offset = 0;
      }
    }
    if (++this.blocks % 8 === 0) this.port.postMessage(this.played);
    return true;
  }
}
registerProcessor('vib-audio', VibAudio);
