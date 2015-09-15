import Random from 'random-seed'
import MarkerIndex from '../src/marker-index'

describe('MarkerIndex', () => {
  it('maintains correct marker positions during randomized insertions and mutations', function () {
    this.timeout(Infinity)

    function write (f) {
      // document.write(f())
    }

    for (let i = 0; i < 10000; i++) {
      let seed = Date.now()
      let assertionMessage = `Random Seed: ${seed}`
      let random = new Random(seed)
      let markerIndex = new MarkerIndex(seed)
      let markers = []
      let idCounter = 65

      for (let j = 0; j < 50; j++) {
        if (j === 8) global.debug = true
        let n = random(10)
        if (n >= 4) { // 60% insert
          performInsert()
        } else if (n >= 2) { // 20% splice
          performSplice()
        } else if (markers.length > 0) {
          performDelete()
        }
        write(() => markerIndex.toHTML())
        write(() => '<hr>')
        verifyUniquePathInvariant()
      }

      verifyRanges()
      testIntersectionQueries()

      function verifyRanges () {
        for (let marker of markers) {
          let range = markerIndex.getRange(marker.id)
          assert.equal(range[0], marker.start, `Marker ${marker.id} start. ` + assertionMessage)
          assert.equal(range[1], marker.end, `Marker ${marker.id} end. ` + assertionMessage)
        }
      }

      function verifyUniquePathInvariant (node, alreadySeen) {
        if (!node) {
          if (markerIndex.root) verifyUniquePathInvariant(markerIndex.root, new Set())
          return
        }

        for (let markerId of node.leftMarkerIds) {
          assert(!alreadySeen.has(markerId), 'Redundant paths. ' + assertionMessage)
        }
        for (let markerId of node.rightMarkerIds) {
          assert(!alreadySeen.has(markerId), 'Redundant paths. ' + assertionMessage)
        }

        if (node.left) {
          let alreadySeenOnLeft = new Set()
          for (let markerId of alreadySeen) {
            alreadySeenOnLeft.add(markerId)
          }
          for (let markerId of node.leftMarkerIds) {
            alreadySeenOnLeft.add(markerId)
          }
          verifyUniquePathInvariant(node.left, alreadySeenOnLeft)
        }

        if (node.right) {
          let alreadySeenOnRight = new Set()
          for (let markerId of alreadySeen) {
            alreadySeenOnRight.add(markerId)
          }
          for (let markerId of node.rightMarkerIds) {
            alreadySeenOnRight.add(markerId)
          }
          verifyUniquePathInvariant(node.right, alreadySeenOnRight)
        }
      }

      function testIntersectionQueries () {
        for (let i = 0; i < 10; i++) {
          let [start, end] = getRange()

          let expectedIds = new Set()
          for (let marker of markers) {
            if (marker.start <= end && start <= marker.end) {
              expectedIds.add(marker.id)
            }
          }

          let actualIds = new Set()
          if (start === 7) global.debug = true
          markerIndex.findIntersecting(start, end, actualIds)

          // console.log('find intersecting', start, end, expectedIds, actualIds);

          assert.equal(actualIds.size, expectedIds.size, assertionMessage)
          for (let id of expectedIds) {
            assert(actualIds.has(id), `Expected ${id} to be in set. ` + assertionMessage)
          }
        }
      }

      function performInsert () {
        let id = String.fromCharCode(idCounter++)
        let [start, end] = getRange()
        let exclusive = !!random(2)
        write(() => `insert ${id}, ${start}, ${end}, exclusive: ${exclusive}`)
        markerIndex.insert(id, start, end)
        if (exclusive) markerIndex.setExclusive(id, true)
        markers.push({id, start, end, exclusive})
      }

      function performSplice () {
        let [start, oldExtent, newExtent] = getSplice()
        write(() => `splice ${start}, ${oldExtent}, ${newExtent}`)
        markerIndex.splice(start, oldExtent, newExtent)
        applySplice(markers, start, oldExtent, newExtent)
      }

      function performDelete () {
        let [{id}] = markers.splice(random(markers.length - 1), 1)
        write(() => `delete ${id}`)
        markerIndex.delete(id)
      }

      function getRange () {
        let start = random(100)
        let end = start
        while (random(3) > 0) {
          end += random.intBetween(-10, 10)
        }
        end = Math.max(end, 0)

        if (start <= end) {
          return [start, end]
        } else {
          return [end, start]
        }
      }

      function getSplice () {
        let [start, oldEnd] = getRange()
        let oldExtent = oldEnd - start
        let newExtent = 0
        while (random(2)) {
          newExtent += random(10)
        }
        return [start, oldExtent, newExtent]
      }

      function applySplice (markers, spliceStart, oldExtent, newExtent) {
        let spliceOldEnd = spliceStart + oldExtent
        let spliceNewEnd = spliceStart + newExtent
        let spliceDelta = newExtent - oldExtent

        for (let marker of markers) {
          let isEmpty = marker.start == marker.end

          if (spliceStart < marker.start || marker.exclusive && spliceOldEnd === marker.start) {
            if (spliceOldEnd <= marker.start) { // splice precedes marker start
              marker.start += spliceDelta
            } else { // splice surrounds marker start
              marker.start = spliceNewEnd
            }
          }

          if (spliceStart < marker.end || (!marker.exclusive || isEmpty) && spliceOldEnd === marker.end) {
            if (spliceOldEnd <= marker.end) { // splice precedes marker end
              marker.end += spliceDelta
            } else { // splice surrounds marker end
              marker.end = spliceNewEnd
            }
          }
        }
      }
    }
  })
})
